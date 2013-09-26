//
//  CameraController.cpp
//  CameraControllerApi
//
//  Created by Tobias Scheck on 10.08.13.
//  Copyright (c) 2013 scheck-media. All rights reserved.
//

#include "CameraController.h"
#include "Base64.h"
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <fcntl.h>
#include <vector>

using namespace CameraControllerApi;
using std::exception;
using std::string;
using std::vector;
using boost::property_tree::ptree;
using boost::archive::iterators::remove_whitespace;
using boost::archive::iterators::transform_width;
using boost::archive::iterators::insert_linebreaks;
using boost::archive::iterators::base64_from_binary;
using boost::archive::iterators::ostream_iterator;

CameraController::CameraController(){
    if(!this->_camera_found){        
        gp_camera_new(&this->_camera);
        this->_ctx = gp_context_new();
        gp_context_set_error_func(this->_ctx, (GPContextErrorFunc)CameraController::_error_callback, NULL);
        gp_context_set_message_func(this->_ctx, (GPContextMessageFunc)CameraController::_message_callback, NULL);
        
        int ret = gp_camera_init(this->_camera, this->_ctx);
        if(ret < GP_OK){            
            gp_camera_free(this->_camera);
        } else {
            this->_camera_found = true;
        }
        this->_is_initialized = true;
    }
}

CameraController::~CameraController(){
    gp_camera_unref(this->_camera);
    gp_context_unref(this->_ctx);
}


bool CameraController::camera_found(){
    return this->_camera_found;
}

bool CameraController::is_initialized(){
    return this->_is_initialized;
}

int CameraController::capture(const char *filename, string &data){
    int fd, ret;
    CameraFile *file;
    CameraFilePath path;
       
    strcpy(path.folder, "/");
	strcpy(path.name, filename);
    
	ret = gp_camera_capture(this->_camera, GP_CAPTURE_IMAGE, &path, this->_ctx);
    
    fd  = open(filename, O_CREAT | O_WRONLY, 0644);
	ret = gp_file_new(&file);
	ret = gp_camera_file_get(this->_camera, path.folder, path.name, GP_FILE_TYPE_NORMAL, file, this->_ctx);
    
    unsigned long int file_size = 0;
    const char *file_data = NULL;

	ret = gp_file_get_data_and_size (file, &file_data, &file_size);

    char *dest = (char*)malloc(file_size * sizeof(char*));
    base64_encode(dest, (char*)file_data, (int)file_size);
    data.append(dest);
    
    ret = gp_camera_file_delete(this->_camera, path.folder, path.name, this->_ctx);
    
    free(dest);
    gp_file_free(file);
    
    
    int waittime = 500;
    CameraEventType type;
    void *eventdata;
    
    printf("Wait for events from camera\n");
    while(1) {
        ret = gp_camera_wait_for_event(this->_camera, waittime, &type, &eventdata, this->_ctx);
        if(type == GP_EVENT_TIMEOUT) {
            break;
        }
        else if (type == GP_EVENT_CAPTURE_COMPLETE) {
            waittime = 100;
        }
        else if (type != GP_EVENT_UNKNOWN) {
            printf("Unexpected event received from camera: %d\n", (int)type);
        }
    }
    
    return true;
}

int CameraController::get_settings(ptree &sett){
    CameraWidget *w, *children;
    int ret;
    ret = gp_camera_get_config(this->_camera, &w, this->_ctx);
    if(ret < GP_OK){
        return false;
    }
    
    ret = gp_widget_get_child_by_name(w, "main", &children);
    if(ret < GP_OK)
        return false;

    this->_read_widget(children, sett, "settings");
    
    return true;
}


int CameraController::get_settings_value(const char *key, void *val){
    CameraWidget *w, *child;
    int ret;
    
    ret = gp_camera_get_config(this->_camera, &w, this->_ctx);
    if(ret < GP_OK){
        return ret;
    }
    
    ret = gp_widget_get_child_by_name(w, key, &child);
    if(ret < GP_OK)
        return ret;
    
    ret = gp_widget_get_value(child, val);
    if(ret < GP_OK)
        return ret;
    
    return true;
}

int CameraController::set_settings_value(const char *key, const char *val){

    CameraWidget *w, *focusWidget;
    int ret = gp_camera_get_config(this->_camera, &w, this->_ctx);
    if(ret < GP_OK)
        return false;
    
    ret = gp_widget_get_child_by_name(w, key, &focusWidget);
    if(ret < GP_OK)
        return false;
    
    ret = gp_widget_set_value(focusWidget, val);
    if(ret < GP_OK)
        return false;
    
    
    ret = gp_camera_set_config(this->_camera, w, this->_ctx);
    
    gp_widget_free(w);
    return (ret == GP_OK);
}

/** borrowed http://stackoverflow.com/a/12784770 */
bool CameraController::_toBase64(char* dest, const char* src, uint size){
    char tail[3] = {0,0,0};
    typedef base64_from_binary<transform_width<const char *, 6, 8> > base64_enc;
    
    uint one_third_len = size/3;
    uint len_rounded_down = one_third_len*3;
    uint j = len_rounded_down + one_third_len;
    std::copy(base64_enc(src), base64_enc(src + len_rounded_down), dest);
    
    if (len_rounded_down != size)
    {
        uint i=0;
        for(; i < size - len_rounded_down; ++i)
        {
            tail[i] = src[len_rounded_down+i];
        }
        
        std::copy(base64_enc(tail), base64_enc(tail + 3), dest + j);
        
        for(i=size + one_third_len + 1; i < j+4; ++i)
        {
            dest[i] = '=';
        }
        
    }
    
    return true;
}


void CameraController::_read_widget(CameraWidget *w,  ptree &tree, string node){
    const char  *name;
    ptree subtree;
    gp_widget_get_name(w, &name);
    string nodename = node + "." + name;
    
    int items = gp_widget_count_children(w);
    if(items > 0){
        for(int i = 0; i < items; i++){
            CameraWidget *item_widget;
            gp_widget_get_child(w, i, &item_widget);
            
            this->_read_widget(item_widget, subtree, nodename);
        }
    } else {
        this->_get_item_value(w, subtree);
    }
    
    tree.put_child(name, subtree);
}


void CameraController::_get_item_value(CameraWidget *w, ptree &tree){
    CameraWidgetType type;
    int id;
    const char *item_label, *item_name;
    ptree item_choices;
    void* item_value;
    gp_widget_get_id(w, &id);
    gp_widget_get_label(w, &item_label);
    gp_widget_get_name(w, &item_name);
    gp_widget_get_value(w, &item_value);
    gp_widget_get_type(w, &type);
    
    int number_of_choices = gp_widget_count_choices(w);
    if(number_of_choices > 0){
        ptree choice_value;
        for(int i = 0; i < number_of_choices; i++){
            const char *choice;
            gp_widget_get_choice(w, i, &choice);
            choice_value.put_value(choice);
            item_choices.push_back(std::make_pair("", choice_value));
        }
    }
    
    

    tree.put("id",              id);
    tree.put("name",            item_name);
    tree.put("label",           item_label);
    tree.put_child("choices",   item_choices);
    
    switch (type) {
        case GP_WIDGET_TEXT:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU: {
            unsigned char *val = static_cast<unsigned char *>(item_value);
            tree.put("value", val);
            break;
        }
        case GP_WIDGET_TOGGLE:{
            //FIXME: problem with casting that value
            /*
             int *val = (int *)item_value;
             if(val)
             tree("value", *val);*/
            break;
        }
            
        case GP_WIDGET_RANGE: {
            //FIXME: problem with casting that value
            /*float *val = (float *)item_value;
             if(val)
             tree("value", *val);*/
            break;
        }
            
        default:
            break;
    }
}

int CameraController::_validate_widget(CameraWidget *w, const char *key, CameraWidget *child){
    return true;
}

GPContextErrorFunc CameraController::_error_callback(GPContext *context, const char *text, void *data){
    
    return 0;
}

GPContextMessageFunc CameraController::_message_callback(GPContext *context, const char *text, void *data){
    
    return 0;
}
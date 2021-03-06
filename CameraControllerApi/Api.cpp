//
//  Api.cpp
//  CameraControllerApi
//
//  Created by Tobias Scheck on 09.08.13.
//  Copyright (c) 2013 scheck-media. All rights reserved.
//

#include "Api.h"
#include "Settings.h"
#include <boost/lexical_cast.hpp>

using namespace CameraControllerApi;

Api::Api(CameraController *cc){
    this->_cc = cc;
}

bool Api::list_settings(CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
   
    ptree settings;
    this->_cc->get_settings(settings);
    Api::buildResponse(settings, type, CCA_API_RESPONSE_SUCCESS, output);
    return true;
}

bool Api::set_focus_point(string focus_point, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    return this->_set_settings_value("d108", focus_point, type, output);
}

bool Api::set_aperture(string aperture, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    return this->_set_settings_value("f-number", aperture, type, output);
}

bool Api::set_speed(string speed, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    return this->_set_settings_value("shutterspeed2", speed, type, output);
}

bool Api::set_iso(string iso, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    return this->_set_settings_value("iso", iso, type, output);
}

bool Api::set_whitebalance(string wb, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    return this->_set_settings_value("whitebalance", wb, type, output);
}

bool Api::shot(CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    ptree tree;
    string image;
    int ret = this->_cc->capture("image.jpg", image);
    if(ret){
        tree.put("image", image);
        Api::buildResponse(tree, type, CCA_API_RESPONSE_SUCCESS, output);
        
    } else {
        Api::buildResponse(tree, type, CCA_API_RESPONSE_INVALID, output);
    }
    
    return true;
}

bool Api::liveview(CCA_API_LIVEVIEW_MODES mode, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    ptree tree;
    if(mode == CCA_API_LIVEVIEW_START){
        int ret = this->_cc->liveview_start();
        if(ret){
            string port, address;
            Settings *sett = Settings::getInstance();
            sett->get_value("preview.remote_port", port);
            sett->get_value("preview.host", address);
            
            tree.put("port", port);
            tree.put("ip_address", address);
            
            Api::buildResponse(tree, type, CCA_API_RESPONSE_SUCCESS, output);
            
        } else {
            Api::buildResponse(tree, type, CCA_API_RESPONSE_INVALID, output);
        }
    } else {
        int ret = this->_cc->liveview_stop();
        if(ret){
            tree.put("stop", "success");
            Api::buildResponse(tree, type, CCA_API_RESPONSE_SUCCESS, output);
            
        } else {
            Api::buildResponse(tree, type, CCA_API_RESPONSE_INVALID, output);
        }

    }
    
    
    return true;
}

bool Api::burst(int number_of_images, CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    int ret = 0;
    ptree tree, images;
    string base64image;
    char filename[256];
    
    for(int i = 0; i < number_of_images; i++){
     
        snprintf(filename, 256, "shot-%04d.jpg", i);
        if(ret){
            ptree image;
            image.put_value(base64image);
            images.push_back(std::make_pair("", image));
        } else {
            return false;
        }
    }
    tree.put_child("preview_images", images);
    
    Api::buildResponse(tree, type, CCA_API_RESPONSE_SUCCESS, output);
    
    return true;
}

bool Api::autofocus(CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNotFound(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
    
    string value;
    this->_cc->get_settings_value("autofocusdrive", (void *)&value);
    if(value.empty()){
        int val = atoi(value.c_str());
        val++;
        value = boost::lexical_cast<string>(val);
    }
    this->_set_settings_value("autofocusdrive", value, type, output);
    return true;
}

bool Api::_buildCameraNotFound(CCA_API_RESPONSE resp, CCA_API_OUTPUT_TYPE type, string &output){    
    ptree n;
    Api::buildResponse(n, type, resp, output);
    return false;
}

void Api::buildResponse(ptree data, CCA_API_OUTPUT_TYPE type, CCA_API_RESPONSE resp, string &output){    
    try{
        boost::property_tree::ptree root;
        std::stringstream ss;
        if(resp != CCA_API_RESPONSE_SUCCESS){
            root.put("cca_response.state", "fail");
            string message;
            Api::errorMessage(resp, message);
            root.put("cca_response.message", message);
        } else {                        
            root.put("cca_response.state", "success");
            string message;
            if(data.empty() == false)
                root.add_child("cca_response.data", data);
        }
        
        if(type == CCA_OUTPUT_TYPE_JSON){
            boost::property_tree::write_json(ss, root);
        } else if(type == CCA_OUTPUT_TYPE_XML){
            boost::property_tree::write_xml(ss, root);
        }
        
        output = ss.str();
    } catch(std::exception &e){
        std::cout<<"Error: " << e.what();
    }
}

bool Api::_set_settings_value(string key, string value, CCA_API_OUTPUT_TYPE type, string &output){
    ptree tree;
    int ret = this->_cc->set_settings_value(key.c_str(), value.c_str());
    if(ret)
        Api::buildResponse(tree, type, CCA_API_RESPONSE_SUCCESS, output);
    else
        Api::buildResponse(tree, type, CCA_API_RESPONSE_INVALID, output);
    
    return ret;
}

void Api::errorMessage(CCA_API_RESPONSE errnr, string &message){
    try {
        boost::property_tree::ptree pt;
        boost::property_tree::read_xml(CCA_ERROR_MESSAGE_FILE, pt);
        std::ostringstream errorstr;
        errorstr << errnr;
        
        string error_id = errorstr.str();

        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("CCA.errors")){
            std::string id = v.second.get_child("<xmlattr>.id").data();
            if(id == error_id){
                message = v.second.data();
                return;
            }
        }
        message = pt.get<std::string>("CCA.errors", 0);
    } catch (std::exception const &e) {
        std::cout<<"Error: " << e.what();
    }
}
//
//  Api.cpp
//  CameraControllerApi
//
//  Created by Tobias Scheck on 09.08.13.
//  Copyright (c) 2013 scheck-media. All rights reserved.
//

#include "Api.h"



using namespace CameraControllerApi;

Api::Api(CameraController *cc){
    this->_cc = cc;
}

bool Api::list_settings(CCA_API_OUTPUT_TYPE type, string &output){
    if(this->_cc->camera_found() == false)
        return this->_buildCameraNot(CCA_API_RESPONSE_CAMERA_NOT_FOUND,type, output);
   
    ptree settings;
    this->_cc->get_settings(settings);
    Api::buildResponse(settings, type, CCA_API_RESPONSE_SUCCESS, output);
    return true;
}

bool Api::set_focus_point(string focus_point, CCA_API_OUTPUT_TYPE type, string &output){
    int ret = this->_cc->set_settings_value("d108", focus_point.c_str());
    return true;
}

bool Api::_buildCameraNot(CCA_API_RESPONSE resp, CCA_API_OUTPUT_TYPE type, string &output){
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

void Api::errorMessage(CCA_API_RESPONSE errnr, string &message){
    try {
        boost::property_tree::ptree pt;
        boost::property_tree::read_xml(CCA_ERROR_MESSAGE_FILE, pt);
        string error_id = std::to_string(errnr);

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
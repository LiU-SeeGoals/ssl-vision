//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
  \file    capture_network.cpp
  \brief   C++ Implementation: CaptureNetwork
  \author  SSL-Vision Contributors, (C) 2025
*/
//========================================================================

#include "capture_network.h"
#include <iostream>
#include <sstream>

CaptureNetwork::CaptureNetwork(VarList *_settings, int default_camera_id, QObject *parent)
    : QObject(parent), CaptureInterface(_settings), is_capturing(false), width(1920), height(1080), capture_format(COLOR_RGB8)
{
    setupVarTypes();
    mvc_connect(settings);
}

CaptureNetwork::~CaptureNetwork()
{
    if (is_capturing) {
        stopCapture();
    }
}

void CaptureNetwork::setupVarTypes()
{
    network_settings = new VarList("Network Settings");
    settings->addChild(network_settings);

    v_url = new VarString("Camera URL", "rtsp://192.168.1.10/axis-media/media.amp");
    v_username = new VarString("Username", "root");
    v_password = new VarString("Password", "");
    v_protocol = new VarStringEnum("Protocol", "RTSP");
    v_protocol->addItem("RTSP");
    v_protocol->addItem("HTTP");
    v_protocol->addItem("HTTPS");

    v_width = new VarInt("Width", 1920, 320, 1920);
    v_height = new VarInt("Height", 1080, 240, 1080);
    v_fps = new VarInt("Frame Rate", 30, 1, 60);
    v_timeout = new VarInt("Timeout (ms)", 5000, 1000, 30000);
    v_auto_reconnect = new VarBool("Auto Reconnect", true);
    v_buffer_size = new VarInt("Buffer Size", 1, 1, 10);

    network_settings->addChild(v_url);
    network_settings->addChild(v_username);
    network_settings->addChild(v_password);
    network_settings->addChild(v_protocol);
    network_settings->addChild(v_width);
    network_settings->addChild(v_height);
    network_settings->addChild(v_fps);
    network_settings->addChild(v_timeout);
    network_settings->addChild(v_auto_reconnect);
    network_settings->addChild(v_buffer_size);

    conversion_settings = new VarList("Conversion Settings");
    settings->addChild(conversion_settings);

    v_colorout = new VarStringEnum("Output Format", "RGB");
    v_colorout->addItem("RGB");
    v_colorout->addItem("YUV422");
    conversion_settings->addChild(v_colorout);
}

void CaptureNetwork::mvc_connect(VarList *group)
{
    vector<VarType *> v = group->getChildren();
    for (unsigned int i = 0; i < v.size(); i++) {
        connect(v[i], SIGNAL(hasChanged(VarType *)), this, SLOT(changed(VarType *)));
        connect(v[i], SIGNAL(wasEdited(VarType *)), this, SLOT(changed(VarType *)));
        VarList *l = dynamic_cast<VarList *>(v[i]);
        if (l != nullptr) {
            mvc_connect(l);
        }
    }
}

void CaptureNetwork::changed(VarType *group)
{
    if (group == v_width || group == v_height || group == v_fps) {
        width = v_width->getInt();
        height = v_height->getInt();

        if (is_capturing) {
            stopCapture();
            startCapture();
        }
    }

    if (group == v_url || group == v_username || group == v_password || group == v_protocol) {
        updateStreamUrl();

        if (is_capturing) {
            stopCapture();
            startCapture();
        }
    }

    if (group == v_colorout) {
        string format = v_colorout->getString();
        if (format == "RGB") {
            capture_format = COLOR_RGB8;
        } else if (format == "YUV422") {
            capture_format = COLOR_YUV422_UYVY;
        }
    }
}

std::string CaptureNetwork::buildStreamUrl()
{
    std::string url = v_url->getString();
    std::string username = v_username->getString();
    std::string password = v_password->getString();

    if (!username.empty()) {
        size_t protocol_pos = url.find("://");
        if (protocol_pos != std::string::npos) {
            std::string protocol = url.substr(0, protocol_pos + 3);
            std::string remainder = url.substr(protocol_pos + 3);

            if (!password.empty()) {
                url = protocol + username + ":" + password + "@" + remainder;
            } else {
                url = protocol + username + "@" + remainder;
            }
        }
    }

    return url;
}

void CaptureNetwork::updateStreamUrl()
{
    stream_url = buildStreamUrl();
}

bool CaptureNetwork::connectToCamera()
{
    mutex.lock();

    updateStreamUrl();

    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap.set(cv::CAP_PROP_FPS, v_fps->getInt());
    cap.set(cv::CAP_PROP_BUFFERSIZE, v_buffer_size->getInt());

    cap.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, v_timeout->getInt());
    cap.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, v_timeout->getInt());

    bool success = cap.open(stream_url);

    if (success) {
        cv::Mat test_frame;
        success = cap.read(test_frame);
        if (success && !test_frame.empty()) {
            currentFrame = test_frame;
            std::cout << "Network camera connected successfully: " << stream_url << std::endl;
        } else {
            std::cerr << "Failed to read frame from network camera: " << stream_url << std::endl;
            cap.release();
            success = false;
        }
    } else {
        std::cerr << "Failed to connect to network camera: " << stream_url << std::endl;
    }

    mutex.unlock();
    return success;
}

void CaptureNetwork::disconnectFromCamera()
{
    mutex.lock();
    if (cap.isOpened()) {
        cap.release();
    }
    mutex.unlock();
}

bool CaptureNetwork::startCapture()
{
    if (is_capturing) {
        return true;
    }

    if (connectToCamera()) {
        is_capturing = true;
        std::cout << "Network camera capture started" << std::endl;
        return true;
    }

    return false;
}

bool CaptureNetwork::stopCapture()
{
    if (!is_capturing) {
        return true;
    }

    is_capturing = false;
    disconnectFromCamera();
    std::cout << "Network camera capture stopped" << std::endl;
    return true;
}

RawImage CaptureNetwork::getFrame()
{
    mutex.lock();

    if (!is_capturing || !cap.isOpened()) {
        mutex.unlock();
        return rawFrame;
    }

    cv::Mat new_frame;
    bool success = cap.read(new_frame);

    if (success && !new_frame.empty()) {
        currentFrame = new_frame;
        convertFrame(currentFrame, rawFrame);
    } else {
        std::cerr << "Failed to read frame from network camera" << std::endl;

        if (v_auto_reconnect->getBool()) {
            std::cout << "Attempting to reconnect to network camera..." << std::endl;
            disconnectFromCamera();
            if (connectToCamera()) {
                std::cout << "Network camera reconnected successfully" << std::endl;
            }
        }
    }

    mutex.unlock();
    return rawFrame;
}

void CaptureNetwork::releaseFrame()
{
    // Nothing special needed for network capture
}

bool CaptureNetwork::convertFrame(const cv::Mat& src, RawImage& target)
{
    if (src.empty()) {
        return false;
    }

    target.setColorFormat(capture_format);
    target.ensure_allocation(capture_format, src.cols, src.rows);
    target.setTime(0.0); // Could implement proper timestamping
    target.setTimeCam(0.0);

    cv::Mat converted;

    if (capture_format == COLOR_RGB8) {
        if (src.channels() == 3) {
            cv::cvtColor(src, converted, cv::COLOR_BGR2RGB);
        } else {
            converted = src.clone();
        }
    } else if (capture_format == COLOR_YUV422_UYVY) {
        if (src.channels() == 3) {
            cv::cvtColor(src, converted, cv::COLOR_BGR2YUV);
        } else {
            converted = src.clone();
        }
    } else {
        converted = src.clone();
    }

    if (converted.isContinuous()) {
        memcpy(target.getData(), converted.data, converted.total() * converted.elemSize());
    } else {
        for (int i = 0; i < converted.rows; ++i) {
            memcpy(target.getData() + i * converted.cols * converted.elemSize(),
                   converted.ptr(i), converted.cols * converted.elemSize());
        }
    }

    return true;
}

bool CaptureNetwork::copyAndConvertFrame(const RawImage& src, RawImage& target)
{
    return CaptureInterface::copyAndConvertFrame(src, target);
}

bool CaptureNetwork::resetBus()
{
    if (is_capturing) {
        stopCapture();
        return startCapture();
    }
    return true;
}

void CaptureNetwork::readAllParameterValues()
{
    // Network cameras don't typically have readable parameters like physical cameras
    // This could be extended to query camera status via HTTP API if needed
}

bool CaptureNetwork::testConnection()
{
    if (is_capturing) {
        return cap.isOpened();
    }

    cv::VideoCapture test_cap;
    updateStreamUrl();

    test_cap.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, v_timeout->getInt());
    bool success = test_cap.open(stream_url);

    if (success) {
        cv::Mat test_frame;
        success = test_cap.read(test_frame) && !test_frame.empty();
        test_cap.release();
    }

    return success;
}

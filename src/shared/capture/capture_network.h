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
  \file    capture_network.h
  \brief   C++ Interface: CaptureNetwork
  \author  SSL-Vision Contributors, (C) 2025
*/
//========================================================================

#ifndef CAPTURE_NETWORK_H
#define CAPTURE_NETWORK_H

#include "captureinterface.h"
#include "VarTypes.h"
#include <opencv2/opencv.hpp>
#include <QMutex>
#include <QObject>
#include <string>

using namespace VarTypes;

/*!
  \class   CaptureNetwork
  \brief   A network camera capture class using OpenCV for RTSP/HTTP streams
  \author  SSL-Vision Contributors, (C) 2025

  This class provides capture abilities for network cameras (IP cameras)
  that support RTSP, HTTP, or other network streaming protocols.
  It is specifically designed to work with AXIS M3005-V network cameras
  but should work with other IP cameras supporting standard protocols.

  The class uses OpenCV's VideoCapture functionality to handle network
  streams and provides full configuration abilities through the VarTypes system.
*/
class CaptureNetwork : public QObject, public CaptureInterface
{
Q_OBJECT

public slots:
    void changed(VarType *group);

protected:
    QMutex mutex;
    bool is_capturing;

    // OpenCV video capture object
    cv::VideoCapture cap;

    // Current frame storage
    RawImage rawFrame;
    cv::Mat currentFrame;

    // Configuration variables
    VarString *v_url;
    VarString *v_username;
    VarString *v_password;
    VarInt *v_width;
    VarInt *v_height;
    VarInt *v_fps;
    VarInt *v_timeout;
    VarStringEnum *v_protocol;
    VarStringEnum *v_colorout;
    VarBool *v_auto_reconnect;
    VarInt *v_buffer_size;

    // Network settings
    VarList *network_settings;
    VarList *conversion_settings;

    // Internal state
    std::string stream_url;
    int width, height;
    ColorFormat capture_format;

    // Helper methods
    bool connectToCamera();
    void disconnectFromCamera();
    std::string buildStreamUrl();
    bool convertFrame(const cv::Mat& src, RawImage& target);

public:
    explicit CaptureNetwork(VarList *_settings = nullptr, int default_camera_id = 0, QObject *parent = nullptr);
    virtual ~CaptureNetwork();

    // CaptureInterface implementation
    virtual RawImage getFrame() override;
    virtual bool isCapturing() override { return is_capturing; }
    virtual void releaseFrame() override;
    virtual bool startCapture() override;
    virtual bool stopCapture() override;
    virtual bool resetBus() override;
    virtual void readAllParameterValues() override;
    virtual bool copyAndConvertFrame(const RawImage& src, RawImage& target) override;
    virtual string getCaptureMethodName() const override { return "Network Camera"; }

    // Network-specific methods
    bool testConnection();
    void updateStreamUrl();

private:
    void setupVarTypes();
    void mvc_connect(VarList *group);
};

#endif // CAPTURE_NETWORK_H
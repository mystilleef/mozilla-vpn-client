/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "captiveportaldetection.h"
#include "captiveportal.h"
#include "captiveportaldetectionimpl.h"
#include "constants.h"
#include "leakdetector.h"
#include "logger.h"
#include "mozillavpn.h"
#include "settingsholder.h"

#ifdef MVPN_WINDOWS
#  include "platforms/windows/windowscaptiveportaldetection.h"
#endif

namespace {
Logger logger(LOG_CAPTIVEPORTAL, "CaptivePortalDetection");
}

CaptivePortalDetection::CaptivePortalDetection() {
  MVPN_COUNT_CTOR(CaptivePortalDetection);

  connect(&m_captivePortalNotifier,
          &CaptivePortalNotifier::notificationCompleted, this,
          &CaptivePortalDetection::notificationCompleted);
}

CaptivePortalDetection::~CaptivePortalDetection() {
  MVPN_COUNT_DTOR(CaptivePortalDetection);
}

void CaptivePortalDetection::initialize() {
  m_active = SettingsHolder::instance()->captivePortalAlert();
}

void CaptivePortalDetection::stateChanged() {
  logger.log() << "Controller/Stability state changed";

  if (!m_active) {
    return;
  }

  if (MozillaVPN::instance()->controller()->state() != Controller::StateOn ||
      MozillaVPN::instance()->connectionHealth()->stability() ==
          ConnectionHealth::Stable) {
    logger.log() << "No captive portal detection required";
    m_impl.reset();
    return;
  }

  logger.log() << "Start the captive portal detection";
#ifdef MVPN_WINDOWS
  m_impl.reset(new WindowsCaptivePortalDetection());
#else
  logger.log() << "This platform does not support captive portal detection yet";
  return;
#endif

  Q_ASSERT(m_impl);

  connect(m_impl.get(), &CaptivePortalDetectionImpl::detectionCompleted, this,
          &CaptivePortalDetection::detectionCompleted);

  m_impl->start();
}

void CaptivePortalDetection::settingsChanged() {
  logger.log() << "Settings has changed";
  m_active = SettingsHolder::instance()->captivePortalAlert();
}

void CaptivePortalDetection::detectionCompleted(bool detected) {
  logger.log() << "Detection completed:" << detected;

  m_impl.reset();

  if (!detected) {
    return;
  }

  m_captivePortalNotifier.notify();
}

void CaptivePortalDetection::notificationCompleted(
    bool disconnectionRequested) {
  logger.log() << "User informed. The disconnection request status:"
               << disconnectionRequested;

  if (!disconnectionRequested) {
    return;
  }

  MozillaVPN::instance()->deactivate();

  // TODO: start the monitoring
}

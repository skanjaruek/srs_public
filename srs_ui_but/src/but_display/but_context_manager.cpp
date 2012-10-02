/******************************************************************************
 * \file
 *
 * $Id: but_context_manager.cpp
 *
 * Copyright (C) Brno University of Technology
 *
 * This file is part of software developed by dcgm-robotics@FIT group.
 *
 * Author: Tomas Lokaj (xlokaj03@stud.fit.vutbr.cz)
 * Supervised by: Michal Spanel (spanel@fit.vutbr.cz)
 * Date: 07/10/2012
 * 
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "but_context_manager.h"

namespace srs_ui_but
{
CButContextManager::CButContextManager(const std::string & name, rviz::VisualizationManager * manager) :
    Display(name, manager)
{
  get_context_client_ = threaded_nh_.serviceClient<srs_env_model::GetContext>(srs_env_model::GetContext_SRV);
  set_context_client_ = threaded_nh_.serviceClient<srs_env_model::SetContext>(srs_env_model::SetContext_SRV);

  sub_ = threaded_nh_.subscribe(srs_env_model::ContextChanged_TOPIC, 1, &CButContextManager::contextChangedCallback,
                                this);
  context_server_enabled_ = false;

  message_renderer_ = new ogre_tools::TextOutput(ID_MESSAGE_RENDERER, 10, 10, 100, 20);
}

CButContextManager::~CButContextManager()
{
}

void CButContextManager::onEnable()
{
  enabled_ = true;
  context_server_enabled_ = checkContextServer();
  if (context_server_enabled_)
    getContext();
}

void CButContextManager::onDisable()
{
  enabled_ = false;
}

void CButContextManager::update(float wall_dt, float ros_dt)
{
  bool enabled = context_server_enabled_;
  context_server_enabled_ = checkContextServer();
  if (!enabled && context_server_enabled_)
    getContext();

  message_renderer_->setText("Collision hazard");

  switch (context_.collision_hazard_tag)
  {
    case srs_env_model::Context::LOW:
      message_renderer_->setColor(Ogre::ColourValue(0.0, 1.0, 0.0, 0.5));
      message_renderer_->setFontSize(16);
      break;
    case srs_env_model::Context::MEDIUM:
      message_renderer_->setColor(Ogre::ColourValue(1.0, 1.0, 0.0, 0.7));
      message_renderer_->setFontSize(18);
      break;
    case srs_env_model::Context::HIGH:
      message_renderer_->setColor(Ogre::ColourValue(0.0, 0.0, 1.0, 0.9));
      message_renderer_->setFontSize(20);
      break;
    case srs_env_model::Context::COLLISION:
      message_renderer_->setColor(Ogre::ColourValue(1.0, 0.0, 0.0, 1.0));
      message_renderer_->setFontSize(24);
      break;
    case srs_env_model::Context::NONE:
    default:
      message_renderer_->setText("");
      message_renderer_->setFontSize(0);
      return;
      break;
  }
}

bool CButContextManager::checkContextServer()
{
  if (!ros::service::exists(srs_env_model::GetContext_SRV, false))
  {
    setStatus(rviz::status_levels::Error, "Context Server", "Context Server is probably not running.");
    return false;
  }
  else
  {
    setStatus(rviz::status_levels::Ok, "Context Server", "Context Server is running.");
    return true;
  }
}

void CButContextManager::createProperties()
{
  rviz::CategoryPropertyWPtr category_context = property_manager_->createCategory("Context", property_prefix_,
                                                                                  parent_category_);
  m_property_status_ = property_manager_->createProperty<rviz::EnumProperty>(
      "Context status", property_prefix_, boost::bind(&CButContextManager::getContextStatus, this),
      boost::bind(&CButContextManager::setContextStatus, this, _1), category_context, this);
  setPropertyHelpText(m_property_status_, "Status");
  rviz::EnumPropertyPtr enum_prop_s = m_property_status_.lock();
  enum_prop_s->addOption("OK", srs_env_model::Context::OK);
  enum_prop_s->addOption("Emergency", srs_env_model::Context::EMERGENCY);

  m_property_action_ = property_manager_->createProperty<rviz::EnumProperty>(
      "Action", property_prefix_, boost::bind(&CButContextManager::getContextAction, this),
      boost::bind(&CButContextManager::setContextAction, this, _1), category_context, this);
  setPropertyHelpText(m_property_action_, "Action");
  rviz::EnumPropertyPtr enum_prop_a = m_property_action_.lock();
  enum_prop_a->addOption("Default", srs_env_model::Context::DEFAULT);
  enum_prop_a->addOption("Grasping", srs_env_model::Context::GRASPING);
  enum_prop_a->addOption("Moving", srs_env_model::Context::MOVING);

  m_property_connection_ = property_manager_->createProperty<rviz::EnumProperty>(
      "Connection", property_prefix_, boost::bind(&CButContextManager::getContextConnection, this),
      boost::bind(&CButContextManager::setContextConnection, this, _1), category_context, this);
  setPropertyHelpText(m_property_connection_, "Connection");
  rviz::EnumPropertyPtr enum_prop_c = m_property_connection_.lock();
  enum_prop_c->addOption("Unknown", srs_env_model::Context::UNKNOWN);
  enum_prop_c->addOption("LAN", srs_env_model::Context::LAN);
  enum_prop_c->addOption("Wifi", srs_env_model::Context::WIFI);

  m_property_collision_hazard_ = property_manager_->createProperty<rviz::EnumProperty>(
      "Collision hazard", property_prefix_, boost::bind(&CButContextManager::getContextCollisionHazard, this),
      boost::bind(&CButContextManager::setContextCollisionHazard, this, _1), category_context, this);
  setPropertyHelpText(m_property_connection_, "Collision hazard");
  rviz::EnumPropertyPtr enum_prop_ch = m_property_collision_hazard_.lock();
  enum_prop_ch->addOption("None", srs_env_model::Context::NONE);
  enum_prop_ch->addOption("Low", srs_env_model::Context::LOW);
  enum_prop_ch->addOption("Medium", srs_env_model::Context::MEDIUM);
  enum_prop_ch->addOption("High", srs_env_model::Context::HIGH);
  enum_prop_ch->addOption("Collision", srs_env_model::Context::COLLISION);

}

}

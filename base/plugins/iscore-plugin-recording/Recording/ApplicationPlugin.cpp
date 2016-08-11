#include "ApplicationPlugin.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <QAction>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>
#include <Recording/Record/RecordManager.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <iscore/application/ApplicationContext.hpp>

#include <core/document/Document.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Curve/Settings/Model.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <QApplication>
#include <QMainWindow>

namespace Recording
{
ApplicationPlugin::ApplicationPlugin(
        const iscore::GUIApplicationContext& ctx) :
    iscore::GUIApplicationContextPlugin {ctx}
{
    using namespace Scenario;
    // Since we have declared the dependency, we can assume
    // that ScenarioApplicationPlugin is instantiated already.
    auto& scenario_plugin = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
    connect(&scenario_plugin.execution(), &ScenarioExecution::startRecording,
            this, &ApplicationPlugin::record);
    connect(&scenario_plugin.execution(), &ScenarioExecution::startRecordingMessages,
            this, &ApplicationPlugin::recordMessages);
    connect(&scenario_plugin.execution(), &ScenarioExecution::stopRecording, // TODO this seems useless
            this, &ApplicationPlugin::stopRecord);


    m_ossiaplug = &ctx.components.applicationPlugin<Engine::ApplicationPlugin>();

    auto& stop_action = ctx.actions.action<Actions::Stop>();
    m_stopAction = stop_action.action();
    connect(m_stopAction, &QAction::triggered,
            this, [&] { stopRecord(); });


}

void ApplicationPlugin::record(
        const Scenario::ProcessModel& scenar,
        Scenario::Point pt)
{
    m_stopAction->trigger();
    QApplication::processEvents();

    Recording::RecordContext ctx{scenar, pt};
    m_recManager = std::make_unique<Recording::RecordManager>(ctx);

    if(context.settings<Curve::Settings::Model>().getPlayWhileRecording())
    {
        connect(m_recManager.get(), &Recording::RecordManager::requestPlay,
                this, [=] ()
        {
            m_ossiaplug->on_record(pt.date);
        }, Qt::QueuedConnection);
    }

    m_recManager->setup();

}

void ApplicationPlugin::recordMessages(
        const Scenario::ProcessModel& scenar,
        Scenario::Point pt)
{
    m_stopAction->trigger();
    QApplication::processEvents();

    Recording::RecordContext ctx{scenar, pt};
    m_recMessagesManager = std::make_unique<Recording::RecordMessagesManager>(ctx);

    if(context.settings<Curve::Settings::Model>().getPlayWhileRecording())
    {
        connect(m_recMessagesManager.get(), &Recording::RecordMessagesManager::requestPlay,
                this, [=] ()
        {
            m_ossiaplug->on_record(pt.date);
        }, Qt::QueuedConnection);
    }

    m_recMessagesManager->setup();
}

void ApplicationPlugin::stopRecord()
{
    if(m_recManager)
    {
        m_recManager->stop();
        m_recManager.reset();
    }

    if(m_recMessagesManager)
    {
        m_recMessagesManager->stop();
        m_recMessagesManager.reset();
    }
}
}
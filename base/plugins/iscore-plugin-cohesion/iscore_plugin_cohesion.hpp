#pragma once
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QObject>
#include <QStringList>
#include <utility>

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

namespace iscore {
class Application;
}  // namespace iscore

/**
 * @brief The IScoreCohesion class
 *
 * This plug-in is here to set-up things that require multiple plug-ins.
 * For instance, if a feature requires the Scenario, the Curve, and the DeviceExplorer,
 * it should certainly be implemented here.
 *
 */
class iscore_plugin_cohesion final :
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::CommandFactory_QtInterface,
        public iscore::PluginRequirementslInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID GUIApplicationContextPlugin_QtInterface_iid)
        Q_INTERFACES(
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::CommandFactory_QtInterface
                iscore::PluginRequirementslInterface_QtInterface
                )

    public:
        iscore_plugin_cohesion();
        ~iscore_plugin_cohesion() = default;

        iscore::GUIApplicationContextPlugin* make_applicationPlugin(
                iscore::Application& app) override;

        std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() override;

        QStringList required() const override;
};


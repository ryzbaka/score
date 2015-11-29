#pragma once

#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

class LayerModelPanelProxy;
class MappingModel;
class QObject;
template <typename tag, typename impl> class id_base_t;

class MappingLayerModel : public LayerModel
{
        ISCORE_METADATA("MappingLayerModel")
        public:
            MappingLayerModel(
                MappingModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        MappingLayerModel(
                const MappingLayerModel& source,
                MappingModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        MappingLayerModel(
                Deserializer<Impl>& vis,
                MappingModel& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const MappingModel& model() const;
};

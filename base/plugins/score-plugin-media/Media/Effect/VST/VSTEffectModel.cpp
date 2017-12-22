#include "VSTEffectModel.hpp"

#include <QUrl>
#include <QFile>
#include <QFileInfo>

#include <ModernMIDI/midi_message.h>
#include <set>
#include <iostream>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <ossia/detail/math.hpp>
#include <score/tools/Todo.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/network/domain/domain.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Media/Effect/VST/VSTNode.hpp>
#include <aeffectx.h>
#include <QTimer>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <websocketpp/base64/base64.hpp>
void show_vst2_editor(AEffect& effect, ERect rect);
void hide_vst2_editor(AEffect& effect);

namespace Media::VST
{
// Taken from VST API
struct HostCanDos
{
    static const constexpr auto canDoSendVstEvents = "sendVstEvents"; ///< Host supports send of Vst events to plug-in
    static const constexpr auto canDoSendVstMidiEvent = "sendVstMidiEvent"; ///< Host supports send of MIDI events to plug-in
    static const constexpr auto canDoSendVstTimeInfo = "sendVstTimeInfo"; ///< Host supports send of VstTimeInfo to plug-in
    static const constexpr auto canDoReceiveVstEvents = "receiveVstEvents"; ///< Host can receive Vst events from plug-in
    static const constexpr auto canDoReceiveVstMidiEvent = "receiveVstMidiEvent"; ///< Host can receive MIDI events from plug-in
    static const constexpr auto canDoReportConnectionChanges = "reportConnectionChanges"; ///< Host will indicates the plug-in when something change in plug-in�s routing/connections with #suspend/#resume/#setSpeakerArrangement
    static const constexpr auto canDoAcceptIOChanges = "acceptIOChanges"; ///< Host supports #ioChanged ()
    static const constexpr auto canDoSizeWindow = "sizeWindow"; ///< used by VSTGUI
    static const constexpr auto canDoOffline = "offline"; ///< Host supports offline feature
    static const constexpr auto canDoOpenFileSelector = "openFileSelector"; ///< Host supports function #openFileSelector ()
    static const constexpr auto canDoCloseFileSelector = "closeFileSelector"; ///< Host supports function #closeFileSelector ()
    static const constexpr auto canDoStartStopProcess = "startStopProcess"; ///< Host supports functions #startProcess () and #stopProcess ()
    static const constexpr auto canDoShellCategory = "shellCategory"; ///< 'shell' handling via uniqueID. If supported by the Host and the Plug-in has the category #kPlugCategShell
    static const constexpr auto canDoSendVstMidiEventFlagIsRealtime = "sendVstMidiEventFlagIsRealtime"; ///< Host supports flags for #VstMidiEvent

    static const constexpr auto canDoHasCockosViewAsConfig = "hasCockosViewAsConfig"; ///< Host supports flags for #VstMidiEvent
};

VSTEffectModel::VSTEffectModel(
    const QString& path,
    const Id<EffectModel>& id,
    QObject* parent):
  EffectModel{id, parent},
  m_effectPath{path}
{
  init();
  create();
}

VSTEffectModel::~VSTEffectModel()
{
  closePlugin();
}

QString VSTEffectModel::prettyName() const
{
  return metadata().getLabel();
}

VSTControlInlet* VSTEffectModel::getControl(const Id<Process::Port>& p)
{
  for(auto e : m_inlets)
    if(e->id() == p)
      return static_cast<VSTControlInlet*>(e);
  return nullptr;
}

void VSTEffectModel::init()
{
  connect(this, &VSTEffectModel::addControl,
          this, &VSTEffectModel::on_addControl);
}

void VSTEffectModel::on_addControl(int i, float v)
{
  auto ctrl = new VSTControlInlet{
              Id<Process::Port>(getStrongId(inlets()).val()), this};
  ctrl->hidden = true;
  ctrl->fxNum = i;
  ctrl->setDomain(ossia::make_domain(0.f, 1.f));
  ctrl->setValue(v);

  // Metadata
  {
    auto name = getString(effGetParamName, i);
    auto label = getString(effGetParamLabel, i);
    // auto display = get_string(effGetParamDisplay, i);

    // Get the nameq
    QString str = name;
    if(!label.isEmpty())
      str += "(" + label + ")";

    ctrl->setCustomData(name);
  }

  connect(ctrl, &Process::ControlInlet::valueChanged,
          this, [this,i] (const ossia::value& v) {
    auto newval =  ossia::convert<float>(v);
    if(std::abs(newval - fx->getParameter(i)) > 0.0001)
      fx->setParameter(i, newval);
  });

  {
    /*
    VstParameterProperties props;
    auto res = dispatch(effGetParameterProperties, i, 0, &props);
    if(res == 1)
    {
      // apparently there's exactly 0 plug-ins supporting this
      qDebug() << props.label << props.minInteger << props.maxInteger << props.smallStepFloat << props.stepFloat;
    }
    */
  }
  m_inlets.push_back(ctrl);
  controls.insert({i, ctrl});
  emit controlAdded(ctrl->id());
}

QString VSTEffectModel::getString(AEffectOpcodes op, int param)
{
  char paramName[512] = {0};
  dispatch(op, param, 0, paramName);
  return QString::fromUtf8(paramName);
}

static auto HostCallback (AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
  VstIntPtr result = 0;

  switch (opcode)
  {
    case audioMasterGetTime:
    {
      auto vst = reinterpret_cast<VSTEffectModel*>(effect->resvd1);
      if(vst)
      {
        result = reinterpret_cast<VstIntPtr>(&vst->fx->info);
      }
      break;
    }
    case audioMasterProcessEvents:
      break;
    case audioMasterIOChanged:
      break;
    case audioMasterSizeWindow:
      break;
    case audioMasterGetInputLatency:
      break;
    case audioMasterGetOutputLatency:
      break;
    case audioMasterIdle:
      effect->dispatcher(effect, effEditIdle, 0, 0, nullptr, 0);
      break;
    case audioMasterVersion :
      result = kVstVersion;
      break;

    case audioMasterCurrentId:
      result = effect->uniqueID;
      break;

    case audioMasterGetSampleRate:
      break;

    case audioMasterGetBlockSize:
      break;

    case audioMasterGetCurrentProcessLevel:
      result = kVstProcessLevelUnknown;
      break;

    case audioMasterAutomate:
    {
      auto vst = reinterpret_cast<VSTEffectModel*>(effect->resvd1);
      if(vst)
      {
        auto ctrl_it = vst->controls.find(index);
        if(ctrl_it != vst->controls.end())
        {
          ctrl_it->second->setValue(opt);
        }
        else
        {
          // TODO use a command;
          emit vst->addControl(index, opt);
        }
      }

      break;
    }
    case audioMasterGetAutomationState:
      result = kVstAutomationUnsupported;
      break;

    case audioMasterGetLanguage:
      result = kVstLangEnglish;
      break;

    case audioMasterGetVendorVersion:
      result = 1;
      break;

    case audioMasterGetVendorString:
      std::copy_n("ossia", 6, static_cast<char*>(ptr));
      result = 1;
      break;

    case audioMasterGetProductString:
      std::copy_n("score", 6, static_cast<char*>(ptr));
      result = 1;
      break;

    case audioMasterUpdateDisplay:
      // TODO update all values
      break;
    case audioMasterBeginEdit:
      break;
    case audioMasterEndEdit:
      break;
    case audioMasterOpenFileSelector:
      break;
    case audioMasterCloseFileSelector:
      break;

    case audioMasterCanDo:
    {
      static const std::set<std::string_view> supported{
        HostCanDos::canDoSendVstEvents,
            HostCanDos::canDoSendVstMidiEvent,
            HostCanDos::canDoSendVstTimeInfo,
            HostCanDos::canDoSendVstMidiEventFlagIsRealtime,
            HostCanDos::canDoHasCockosViewAsConfig
      };
      if(supported.find(static_cast<const char*>(ptr)) != supported.end())
        result = 1;
      break;
    }
  }
  return result;
}
void VSTEffectModel::closePlugin()
{
  if(fx)
  {
    hideUI();
    fx = nullptr;
  }
  qDeleteAll(m_inlets);
  qDeleteAll(m_outlets);
  m_inlets.clear();
  m_outlets.clear();
  metadata().setLabel("Dead VST");
}

void VSTEffectModel::showUI()
{
  ERect* vstRect{};

  dispatch(effEditGetRect, 0, 0, &vstRect, 0.0f);

  int w{};
  int h{};
  if(vstRect)
  {
    w = vstRect->right - vstRect->left;
    h = vstRect->bottom - vstRect->top;
  }

  if(w <= 1)
    w = 640;
  if(h <= 1)
    h = 480;

  bool had_ui = bool(ui);
  show_vst2_editor(*fx->fx, *vstRect);
  if(!had_ui && ui)
  {
    auto& ctx = score::IDocument::documentContext(*this);
    connect(&ctx.coarseUpdateTimer, &QTimer::timeout,
            this, [=] {
      dispatch(effEditIdle);
    }, Qt::UniqueConnection);
  }
}

void VSTEffectModel::hideUI()
{
  hide_vst2_editor(*fx->fx);
}

VSTModule* getPlugin(QString path)
{
  auto path_std = path.toStdString();
  VSTModule* plugin;
  auto& app = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  auto it = app.vst_modules.find(path_std);
  if(it == app.vst_modules.end())
  {
    if(path.isEmpty())
      return nullptr;

    bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
    if(!isFile)
    {
      qDebug() << "Invalid path: " << path;
      return nullptr;
    }

    try {
      plugin = new VSTModule{path_std};
      app.vst_modules.insert({std::move(path_std), plugin});
    } catch(const std::runtime_error& e) {
      qDebug() << e.what();
      return nullptr;
    }
  }
  else
  {
    plugin = it->second;
  }

  return plugin;
}

void VSTEffectModel::initFx(VSTModule& plugin)
{
  auto main = plugin.getMain();
  if(!main) {
    qDebug() << "plugin has no main";
    return;
  }

  fx = std::make_shared<AEffectWrapper>(main(HostCallback));
  if(!fx->fx)
  {
    qDebug() << "plugin was not created";
    fx.reset();
    return;
  }

  fx->fx->resvd1 = reinterpret_cast<VstIntPtr>(this);

  dispatch(effOpen);

  {
    char buf[256] = {0};
    dispatch(effGetProductString, 0, 0, buf);
    QString s = buf;
    if(!s.isEmpty())
      metadata().setLabel(s);
    else
      metadata().setLabel(QFileInfo(m_effectPath).baseName());
  }
}

void VSTEffectModel::create()
{
  SCORE_ASSERT(!fx);
  VSTModule* plugin = getPlugin(m_effectPath);
  if(!plugin)
    return;

  initFx(*plugin);
  if(!fx)
    return;

  int inlet_i = 0;
  if(fx->fx->flags & effFlagsIsSynth)
  {
    m_inlets.push_back(new Process::Inlet(Id<Process::Port>{inlet_i++}, this));
    m_inlets[0]->type = Process::PortType::Midi;
  }
  else
  {
    m_inlets.push_back(new Process::Inlet(Id<Process::Port>{inlet_i++}, this));
    m_inlets[0]->type = Process::PortType::Audio;
  }

  // Tempo
  {
    auto tempo = new Process::ControlInlet(Id<Process::Port>{inlet_i++}, this);
    m_inlets.push_back(tempo);
    tempo->type = Process::PortType::Message;
    tempo->setCustomData("Tempo");
    tempo->setValue(120.);
    tempo->setDomain(ossia::make_domain(20., 200.));
  }
  {
    // Signature
    auto sig = new Process::ControlInlet(Id<Process::Port>{inlet_i++}, this);
    m_inlets.push_back(sig);
    sig->type = Process::PortType::Message;
    sig->setCustomData("Time signature");
    sig->setValue("4/4");
  }

  if(fx->fx->numParams < 10)
  {
    for(int i = 0; i < 10; i++)
    {
      on_addControl(i, fx->getParameter(i));
    }
  }

  m_outlets.push_back(new Process::Outlet(Id<Process::Port>{}, this));
  m_outlets[0]->type = Process::PortType::Audio;
  m_outlets[0]->setPropagate(true);
}

void VSTEffectModel::load()
{
  SCORE_ASSERT(!fx);
  VSTModule* plugin = getPlugin(m_effectPath);
  if(!plugin)
    return;

  initFx(*plugin);
  if(!fx)
    return;

  for(int i = 3; i < m_inlets.size(); i++)
  {
    auto inlet = safe_cast<VSTControlInlet*>(m_inlets[i]);
    int ctrl = inlet->fxNum;
    connect(inlet, &Process::ControlInlet::valueChanged,
            this, [this,ctrl] (const ossia::value& v) {
      auto newval =  ossia::convert<float>(v);
      if(std::abs(newval - fx->getParameter(ctrl)) > 0.0001)
        fx->setParameter(ctrl, newval);
    });
    controls.insert({ctrl, inlet});
  }
}



VSTGraphicsSlider::VSTGraphicsSlider(AEffect* fx, int num, QGraphicsItem* parent):
  QGraphicsItem{parent}
{
  this->fx = fx;
  this->num = num;
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void VSTGraphicsSlider::setRect(QRectF r)
{
  prepareGeometryChange();
  m_rect = r;
}

void VSTGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double VSTGraphicsSlider::value() const
{
  return m_value;
}

void VSTGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  if(curPos != m_value)
  {
    m_value = curPos;
    emit valueChanged(m_value);
    emit sliderMoved();
    update();
  }

  event->accept();
}

void VSTGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    if(curPos != m_value)
    {
      m_value = curPos;
      emit valueChanged(m_value);
      emit sliderMoved();
      update();
    }
  }
  event->accept();
}

void VSTGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    if(curPos != m_value)
    {
      m_value = curPos;
      emit valueChanged(m_value);
      update();
    }
    emit sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF VSTGraphicsSlider::boundingRect() const
{
  return m_rect;
}

void VSTGraphicsSlider::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  static const QPen darkPen{skin.HalfDark.color()};
  static const QPen grayPen{skin.Gray.color()};
  painter->setPen(darkPen);
  painter->setBrush(skin.Dark);

  // Draw rect
  const auto srect = sliderRect();
  painter->drawRoundedRect(srect, 1, 1);

  // Draw text
  painter->setPen(grayPen);
  char str[256]{};
  fx->dispatcher(fx, effGetParamDisplay, num, 0, str, m_value);
  painter->drawText(srect.adjusted(6, -2, -6, -1),
                    QString::fromUtf8(str),
                    getHandleX() > srect.width() / 2 ? QTextOption() : QTextOption(Qt::AlignRight));

  // Draw handle
  painter->setBrush(skin.HalfLight);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(handleRect());
}

bool VSTGraphicsSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double VSTGraphicsSlider::getHandleX() const
{
  return 4 + sliderRect().width() * m_value;
}

QRectF VSTGraphicsSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF VSTGraphicsSlider::handleRect() const
{
  return {getHandleX() - 4., 1., 8., m_rect.height() - 1};
}



struct VSTFloatSlider : Control::ControlInfo
{
    static QGraphicsItem* make_item(AEffect* fx, VSTControlInlet& inlet, const score::DocumentContext& ctx, QWidget* parent, QObject* context)
    {
      auto sl = new VSTGraphicsSlider{fx, inlet.fxNum, nullptr};
      sl->setRect({0., 0., 150., 15.});
      sl->setValue(ossia::convert<double>(inlet.value()));

      QObject::connect(sl, &VSTGraphicsSlider::sliderMoved,
                       context, [=,&inlet,&ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<Control::SetControlValue>(inlet, sl->value());
      });
      QObject::connect(sl, &VSTGraphicsSlider::sliderReleased,
                       context, [&ctx,sl] () {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

      QObject::connect(&inlet, &Process::ControlInlet::valueChanged,
                       sl, [=] (ossia::value val) {
        if(!sl->moving)
          sl->setValue(ossia::convert<double>(val));
      });

      return sl;
    }
};


VSTEffectItem::VSTEffectItem(const VSTEffectModel& effect, const score::DocumentContext& doc, Control::RectItem* root):
  Control::EffectItem{root}
{
  rootItem = root;
  using namespace Control::Widgets;
  QObject::connect(
        &effect, &Process::EffectModel::controlAdded,
        this, [&] (const Id<Process::Port>& id) {
    auto inlet = safe_cast<VSTControlInlet*>(effect.inlet(id));
    setupInlet(effect, *inlet, doc);
    rootItem->setRect(rootItem->childrenBoundingRect());
  });

  {
    auto tempo = safe_cast<Process::ControlInlet*>(effect.inlets()[1]);
    setupInlet(TempoChooser(), *tempo, doc);
  }
  {
    auto sg = safe_cast<Process::ControlInlet*>(effect.inlets()[2]);
    setupInlet(TimeSigChooser(), *sg, doc);
  }
  for(int i = 3; i < effect.inlets().size(); i++)
  {
    auto inlet = safe_cast<VSTControlInlet*>(effect.inlets()[i]);
    setupInlet(effect, *inlet, doc);
  }
}

void VSTEffectItem::setupInlet(const VSTEffectModel& fx, VSTControlInlet& inlet, const score::DocumentContext& doc)
{
  auto item = new Control::RectItem{this};

  double pos_y = this->childrenBoundingRect().height();

  auto port = Dataflow::setupInlet(inlet, doc, item, this);

  auto lab = new Scenario::SimpleTextItem{item};
  lab->setColor(ScenarioStyle::instance().EventDefault);
  lab->setText(inlet.customData());
  lab->setPos(15, 2);


  QGraphicsItem* widg = VSTFloatSlider::make_item(fx.fx->fx, inlet, doc, nullptr, this);
  widg->setParentItem(item);
  widg->setPos(15, lab->boundingRect().height());

  auto h = std::max(20., (qreal)(widg->boundingRect().height() + lab->boundingRect().height() + 2.));

  port->setPos(7., h / 2.);

  item->setPos(0, pos_y);
  item->setRect(QRectF{0., 0, 170., h});
}

}

template <>
void DataStreamReader::read(
    const Media::VST::VSTEffectModel& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  m_stream << eff.effect();

  // TODO save & reload program parameters
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    Media::VST::VSTEffectModel& eff)
{
  writePorts(*this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  m_stream >> eff.m_effectPath;
  // TODO save & reload program parameters
  eff.load();
  checkDelimiter();
}

template <>
void JSONObjectReader::read(
    const Media::VST::VSTEffectModel& eff)
{
  readPorts(obj, eff.m_inlets, eff.m_outlets);
  obj["Effect"] = eff.effect();

  if(eff.fx)
  {
    if(eff.fx->fx->flags & effFlagsProgramChunks)
    {
      void* ptr{};
      auto res = eff.fx->dispatch(effGetChunk, 0, 0, &ptr, 0.f);
      if(ptr && res > 0)
      {
        auto encoded = websocketpp::base64_encode((const unsigned char*)ptr, res);
        obj["Data"] = QString::fromStdString(encoded);
      }
    }
    else
    {
      QJsonArray arr;
      for(int i = 0; i < eff.fx->fx->numParams; i++)
        arr.push_back(eff.fx->getParameter(i));
      obj["Params"] = std::move(arr);
    }
  }
}

template <>
void JSONObjectWriter::write(
    Media::VST::VSTEffectModel& eff)
{
  writePorts(obj, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  eff.m_effectPath = obj["Effect"].toString();
  eff.load();
  QPointer<Media::VST::VSTEffectModel> ptr = &eff;
  QTimer::singleShot(1000, [obj=this->obj,ptr] {
    if(!ptr)
      return;
    auto& eff = *ptr;
    if(eff.fx)
    {
      if(eff.fx->fx->flags & effFlagsProgramChunks)
      {
        auto it = obj.find("Data");
        if(it != obj.end())
        {
          auto b64 = websocketpp::base64_decode(it->toString().toStdString());
          eff.fx->dispatch(effSetChunk, 0, b64.size(), b64.data(), 0.f);

          for(std::size_t i = 3; i < eff.inlets().size(); i++)
          {
            auto inlet = safe_cast<Media::VST::VSTControlInlet*>(eff.inlets()[i]);
            inlet->setValue(eff.fx->getParameter(inlet->fxNum));
          }
        }
      }
      else
      {
        auto it = obj.find("Params");
        if(it != obj.end())
        {
          QJsonArray arr = it->toArray();
          for(int i = 0; i < arr.size(); i++)
          {
            eff.fx->setParameter(i, arr[i].toDouble());
          }
        }
      }
    }});
}

namespace Engine::Execution
{
VSTEffectComponent::VSTEffectComponent(
    Media::VST::VSTEffectModel& proc,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : Engine::Execution::EffectComponent_T<Media::VST::VSTEffectModel>{proc, ctx, id, parent}
{
  AEffect& fx = *proc.fx->fx;
  auto setup_controls = [&] (auto& node) {
    node->ctrl_ptrs.reserve(proc.controls.size());
    for(auto& ctrl : proc.controls)
    {
      auto inlet = ossia::make_inlet<ossia::value_port>();
      node->ctrl_ptrs.push_back({ctrl.second->fxNum, inlet->data.target<ossia::value_port>()});
      node->inputs().push_back(std::move(inlet));
    }

    std::weak_ptr<std::remove_reference_t<decltype(*node)>> wp = node;
    connect(&proc, &Media::VST::VSTEffectModel::controlAdded,
            this, [this,&proc,wp] (const Id<Process::Port>& id) {
      auto port = proc.getControl(id);
      if(!port)
        return;
      if(auto n = wp.lock())
      {
        in_exec([n,num=port->fxNum] {
          auto inlet = ossia::make_inlet<ossia::value_port>();

          n->ctrl_ptrs.push_back({num, inlet->data.target<ossia::value_port>()});
          n->inputs().push_back(inlet);
        });
      }
    });
  };

  if(fx.flags & effFlagsCanDoubleReplacing)
  {
    if(fx.flags & effFlagsIsSynth)
    {
      auto n = Media::VST::make_vst_fx<true, true>(proc.fx, ctx.plugin.execState.sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
    else
    {
      auto n = Media::VST::make_vst_fx<true, false>(proc.fx, ctx.plugin.execState.sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
  }
  else
  {
    if(fx.flags & effFlagsIsSynth)
    {
      auto n = Media::VST::make_vst_fx<false, true>(proc.fx, ctx.plugin.execState.sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
    else
    {
      auto n = Media::VST::make_vst_fx<false, false>(proc.fx, ctx.plugin.execState.sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
  }
}
}


template<>
void DataStreamReader::read<Media::VST::VSTControlInlet>(const Media::VST::VSTControlInlet& p)
{
  read((Process::ControlInlet&)p);
  m_stream << p.fxNum;
}
template<>
void DataStreamWriter::write<Media::VST::VSTControlInlet>(Media::VST::VSTControlInlet& p)
{
  m_stream >> p.fxNum;
}

template<>
void JSONObjectReader::read<Media::VST::VSTControlInlet>(const Media::VST::VSTControlInlet& p)
{
  read((Process::ControlInlet&)p);
  obj["FxNum"] = p.fxNum;
}
template<>
void JSONObjectWriter::write<Media::VST::VSTControlInlet>(Media::VST::VSTControlInlet& p)
{
  p.fxNum = obj["FxNum"].toInt();
}
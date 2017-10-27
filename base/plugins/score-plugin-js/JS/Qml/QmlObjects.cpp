#include "QmlObjects.hpp"

namespace JS
{

ValueInlet::ValueInlet(QObject* parent): Inlet{parent} {}

ValueInlet::~ValueInlet() {}

QVariant ValueInlet::value() const
{
  return m_value;
}

void ValueInlet::setValue(QVariant value)
{
  if (m_value == value)
    return;

  m_value = value;
  emit valueChanged(m_value);
}

ValueOutlet::ValueOutlet(QObject* parent): Outlet{parent} {}

ValueOutlet::~ValueOutlet() {}

QVariant ValueOutlet::value() const
{
  return m_value;
}

void ValueOutlet::setValue(QVariant value)
{
  if (m_value == value)
    return;

  m_value = value;
  emit valueChanged(m_value);
}

AudioInlet::AudioInlet(QObject* parent): Inlet{parent} {}

AudioInlet::~AudioInlet() { }

const QVector<QVector<double> >&AudioInlet::audio() const
{ return m_audio; }

void AudioInlet::setAudio(const QVector<QVector<double> >& audio)
{
  m_audio = audio;
}

AudioOutlet::AudioOutlet(QObject* parent): Outlet{parent} {}

AudioOutlet::~AudioOutlet() { }

const QVector<QVector<double> >&AudioOutlet::audio() const
{ return m_audio; }

Inlet::~Inlet()
{

}

Outlet::~Outlet()
{

}

}
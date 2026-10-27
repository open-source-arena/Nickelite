#!/bin/sh
set -eu
python3 - <<'PY'
from pathlib import Path
p=Path('/tc/src/qtbase/src/platformsupport/fbconvenience/qfbwindow.cpp')
s=p.read_text()
before='''    Q_UNUSED(region);
    Q_UNUSED(flags);
}'''
after='''    Q_UNUSED(flags);
    repaint(region);
}'''
assert s.count(before)==1
p.write_text(s.replace(before,after))
p=Path('/tc/src/qtbase/src/plugins/platforms/vnc/qvncclient.cpp')
s=p.read_text().replace('#include <QtGui/qguiapplication.h>','#include <QtGui/qguiapplication.h>\n#include <QtGui/QPointingDevice>')
before='''        QWindowSystemInterface::handleMouseEvent(nullptr, pos, pos, ev.buttons, Qt::MouseButton(buttonStateChange),
                                                 type, QGuiApplication::keyboardModifiers());'''
after='''        static const QPointingDevice *touchDevice = [] {
            auto *device = new QPointingDevice(QStringLiteral("VNC touchscreen"), 1234,
                QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
                QInputDevice::Capability::Position | QInputDevice::Capability::Area, 1, 0);
            device->setParent(QCoreApplication::instance());
            QWindowSystemInterface::registerInputDevice(device);
            return device;
        }();
        const bool wasDown = buttonState & int(Qt::LeftButton);
        const bool isDown = ev.buttons & Qt::LeftButton;
        if (wasDown || isDown) {
            QWindowSystemInterface::TouchPoint point;
            point.id = 0;
            point.area = QRectF(pos.x() - 1, pos.y() - 1, 2, 2);
            const auto size = m_server->screen()->geometry().size();
            point.normalPosition = QPointF(pos.x() / size.width(), pos.y() / size.height());
            point.pressure = isDown ? 1 : 0;
            point.state = !wasDown ? QEventPoint::State::Pressed
                : !isDown ? QEventPoint::State::Released
                : QEventPoint::State::Updated;
            QWindowSystemInterface::handleTouchEvent(nullptr, touchDevice, {point},
                                                      QGuiApplication::keyboardModifiers());
        }'''
assert s.count(before)==1
s=s.replace(before,after).replace('        int buttonStateChange = buttonState ^ int(ev.buttons);\n','')
s=s.replace('''        QEvent::Type type = QEvent::MouseMove;
        if (int(ev.buttons) > buttonState)
            type = QEvent::MouseButtonPress;
        else if (int(ev.buttons) < buttonState)
            type = QEvent::MouseButtonRelease;
''','')
p.write_text(s)
PY
cmake --build /tc/src/target --target QVncIntegrationPlugin --parallel 4
cp /tc/src/target/plugins/platforms/libqvnc.so /out/libqvnc-repaint.so

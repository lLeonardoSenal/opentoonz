#pragma once

#ifndef REF3DPANEL_H
#define REF3DPANEL_H

#include "toonzqt/glwidget_for_highdpi.h"

#include <QWidget>
#include <QPoint>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

//=============================================================================
// Ref3DViewport
//   Navigation view of the 3D Reference, framed with the aspect ratio of the
//   scene camera. Drag to orbit, Shift+drag or middle drag to pan, right drag
//   or wheel to zoom.
//-----------------------------------------------------------------------------

class Ref3DViewport final : public GLWidgetForHighDpi {
  Q_OBJECT

  QPoint m_lastPos;

public:
  Ref3DViewport(QWidget *parent = nullptr);

protected:
  void initializeGL() override;
  void paintGL() override;

  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;

private:
  double cameraAspect() const;
  QRect frameRect() const;  //!< letterboxed camera area, in device pixels
};

//=============================================================================
// Ref3DPanel
//-----------------------------------------------------------------------------

class Ref3DPanel final : public QWidget {
  Q_OBJECT

  Ref3DViewport *m_viewport;
  QLabel *m_modelLabel, *m_frameLabel, *m_keysLabel;
  QPushButton *m_importButton, *m_clearButton, *m_frameButton;
  QPushButton *m_setKeyButton, *m_removeKeyButton, *m_prevKeyButton,
      *m_nextKeyButton;
  QCheckBox *m_visibleCheck, *m_gridCheck, *m_autoKeyCheck;
  QSpinBox *m_focalField;
  QDoubleSpinBox *m_rollField;
  QSlider *m_opacitySlider;
  QComboBox *m_shadingCombo, *m_easingCombo;

public:
  Ref3DPanel(QWidget *parent = nullptr);

protected slots:
  void updateControls();
  void onImport();
  void onCameraFieldChanged();
  void onPrevKey();
  void onNextKey();
};

#endif  // REF3DPANEL_H

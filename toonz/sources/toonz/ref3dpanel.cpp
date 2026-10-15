#include "ref3dpanel.h"

#include "ref3dscene.h"
#include "tapp.h"

// TnzLib includes
#include "toonz/tcamera.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"

// TnzCore includes
#include "tgl.h"

// Qt includes
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace {

int currentFrame() { return TApp::instance()->getCurrentFrame()->getFrame(); }

}  // namespace

//=============================================================================
// Ref3DViewport
//-----------------------------------------------------------------------------

Ref3DViewport::Ref3DViewport(QWidget *parent) : GLWidgetForHighDpi(parent) {
  QSurfaceFormat fmt = format();
  fmt.setDepthBufferSize(24);
  fmt.setSamples(4);
  setFormat(fmt);
  setMinimumSize(160, 120);
  setFocusPolicy(Qt::ClickFocus);
  connect(Ref3DScene::instance(), &Ref3DScene::changed, this,
          [this]() { update(); });
}

void Ref3DViewport::initializeGL() { initializeOpenGLFunctions(); }

double Ref3DViewport::cameraAspect() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TCamera *camera   = scene ? scene->getCurrentCamera() : nullptr;
  double aspect     = camera ? camera->getAspectRatio() : 16.0 / 9.0;
  return aspect > 0 ? aspect : 16.0 / 9.0;
}

QRect Ref3DViewport::frameRect() const {
  int w = width() * getDevPixRatio(), h = height() * getDevPixRatio();
  double aspect = cameraAspect();
  int fw = w, fh = int(w / aspect);
  if (fh > h) fh = h, fw = int(h * aspect);
  return QRect((w - fw) / 2, (h - fh) / 2, fw, fh);
}

void Ref3DViewport::paintGL() {
  int w = width() * getDevPixRatio(), h = height() * getDevPixRatio();
  glViewport(0, 0, w, h);
  glClearColor(0.16f, 0.16f, 0.16f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // camera frame (GL origin is bottom-left, the letterbox is symmetric)
  QRect r = frameRect();
  glEnable(GL_SCISSOR_TEST);
  glScissor(r.x(), r.y(), r.width(), r.height());
  glClearColor(0.82f, 0.82f, 0.82f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);

  Ref3DScene *ref = Ref3DScene::instance();
  if (ref->hasModel()) {
    glViewport(r.x(), r.y(), r.width(), r.height());
    ref->draw(ref->currentCamera(currentFrame()), cameraAspect());
    glViewport(0, 0, w, h);
  }

  QPainter p(this);
  p.setPen(QColor(90, 90, 90));
  QString text;
  int frame = currentFrame();
  if (!ref->hasModel())
    text = tr("Import a .glb, .gltf or .obj model exported from Blender");
  else if (ref->isKey(frame))
    text = tr("Key");
  if (!text.isEmpty())
    p.drawText(rect().adjusted(8, 8, -8, -8),
               Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, text);
}

void Ref3DViewport::mousePressEvent(QMouseEvent *e) { m_lastPos = e->pos(); }

void Ref3DViewport::mouseMoveEvent(QMouseEvent *e) {
  Ref3DScene *ref = Ref3DScene::instance();
  if (!ref->hasModel()) return;
  QPoint d   = e->pos() - m_lastPos;
  m_lastPos  = e->pos();
  int frame  = currentFrame();
  Ref3DCamera cam = ref->currentCamera(frame);

  bool pan = (e->buttons() & Qt::MiddleButton) ||
             ((e->buttons() & Qt::LeftButton) &&
              (e->modifiers() & Qt::ShiftModifier));
  bool zoom = (e->buttons() & Qt::RightButton) ||
              ((e->buttons() & Qt::LeftButton) &&
               (e->modifiers() & Qt::ControlModifier));

  if (pan) {
    // move the target in the camera plane, at the speed of the cursor
    QMatrix4x4 view = cam.viewMatrix();
    QVector3D right(view(0, 0), view(0, 1), view(0, 2));
    QVector3D up(view(1, 0), view(1, 1), view(1, 2));
    double halfFov  = std::atan(18.0 / std::max(1.0, cam.m_focal));
    double unit     = 2.0 * cam.m_distance * std::tan(halfFov) /
                  std::max(1, frameRect().width() / int(getDevPixRatio()));
    cam.m_target += float(unit) * (-d.x() * right + d.y() * up);
  } else if (zoom) {
    cam.m_distance *= std::pow(1.01, d.y());
  } else if (e->buttons() & Qt::LeftButton) {
    cam.m_yaw -= d.x() * 0.4;
    cam.m_pitch = std::max(-89.0, std::min(89.0, cam.m_pitch + d.y() * 0.4));
  } else
    return;
  ref->setFreeCamera(cam, frame);
}

void Ref3DViewport::wheelEvent(QWheelEvent *e) {
  Ref3DScene *ref = Ref3DScene::instance();
  if (!ref->hasModel()) return;
  int frame       = currentFrame();
  Ref3DCamera cam = ref->currentCamera(frame);
  double steps    = e->angleDelta().y() / 120.0;
  cam.m_distance *= std::pow(0.9, steps);
  ref->setFreeCamera(cam, frame);
  e->accept();
}

//=============================================================================
// Ref3DPanel
//-----------------------------------------------------------------------------

Ref3DPanel::Ref3DPanel(QWidget *parent) : QWidget(parent) {
  Ref3DScene *ref = Ref3DScene::instance();

  m_viewport     = new Ref3DViewport(this);
  m_importButton = new QPushButton(tr("Import Model..."), this);
  m_clearButton  = new QPushButton(tr("Remove"), this);
  m_frameButton  = new QPushButton(tr("Frame Model"), this);
  m_modelLabel   = new QLabel(this);
  m_visibleCheck = new QCheckBox(tr("Show in Viewer"), this);

  m_focalField = new QSpinBox(this);
  m_focalField->setRange(8, 300);
  m_focalField->setSuffix(" mm");
  m_rollField = new QDoubleSpinBox(this);
  m_rollField->setRange(-180, 180);
  m_rollField->setDecimals(1);
  m_rollField->setSuffix(QString::fromUtf8("\xc2\xb0"));
  m_opacitySlider = new QSlider(Qt::Horizontal, this);
  m_opacitySlider->setRange(0, 100);
  m_opacitySlider->setMinimumWidth(80);
  m_shadingCombo = new QComboBox(this);
  m_shadingCombo->addItems(
      {tr("Solid"), tr("Wireframe"), tr("Solid + Wireframe")});
  m_gridCheck = new QCheckBox(tr("Ground Grid"), this);

  m_frameLabel      = new QLabel(this);
  m_setKeyButton    = new QPushButton(tr("Set Key"), this);
  m_removeKeyButton = new QPushButton(tr("Remove Key"), this);
  m_prevKeyButton   = new QPushButton(tr("< Key"), this);
  m_nextKeyButton   = new QPushButton(tr("Key >"), this);
  m_easingCombo     = new QComboBox(this);
  m_easingCombo->addItems({tr("Linear"), tr("Ease In/Out")});
  m_autoKeyCheck = new QCheckBox(tr("Auto Key"), this);
  m_keysLabel    = new QLabel(this);
  m_keysLabel->setWordWrap(true);

  m_setKeyButton->setToolTip(
      tr("Store the current camera on the current frame"));
  m_autoKeyCheck->setToolTip(
      tr("Every camera move is stored on the current frame"));
  m_visibleCheck->setToolTip(
      tr("Show the model behind the drawings in the viewers"));

  QLabel *help = new QLabel(
      tr("Drag: orbit   Shift+drag or middle button: pan   "
         "Right drag or wheel: zoom"),
      this);
  help->setWordWrap(true);
  help->setEnabled(false);

  //---- layout
  QVBoxLayout *mainLay = new QVBoxLayout(this);
  mainLay->setMargin(4);
  mainLay->setSpacing(4);
  {
    QHBoxLayout *lay = new QHBoxLayout();
    lay->addWidget(m_importButton);
    lay->addWidget(m_modelLabel, 1);
    lay->addWidget(m_frameButton);
    lay->addWidget(m_clearButton);
    lay->addWidget(m_visibleCheck);
    mainLay->addLayout(lay);
  }
  mainLay->addWidget(m_viewport, 1);
  {
    QHBoxLayout *lay = new QHBoxLayout();
    lay->addWidget(new QLabel(tr("Lens:"), this));
    lay->addWidget(m_focalField);
    lay->addWidget(new QLabel(tr("Roll:"), this));
    lay->addWidget(m_rollField);
    lay->addWidget(new QLabel(tr("Opacity:"), this));
    lay->addWidget(m_opacitySlider, 1);
    lay->addWidget(m_shadingCombo);
    lay->addWidget(m_gridCheck);
    mainLay->addLayout(lay);
  }
  {
    QHBoxLayout *lay = new QHBoxLayout();
    lay->addWidget(m_frameLabel);
    lay->addWidget(m_prevKeyButton);
    lay->addWidget(m_setKeyButton);
    lay->addWidget(m_removeKeyButton);
    lay->addWidget(m_nextKeyButton);
    lay->addWidget(m_easingCombo);
    lay->addWidget(m_autoKeyCheck);
    lay->addStretch(1);
    mainLay->addLayout(lay);
  }
  mainLay->addWidget(m_keysLabel);
  mainLay->addWidget(help);

  //---- connections
  connect(m_importButton, &QPushButton::clicked, this, &Ref3DPanel::onImport);
  connect(m_clearButton, &QPushButton::clicked, ref, &Ref3DScene::clearModel);
  connect(m_frameButton, &QPushButton::clicked, this,
          [ref]() { ref->frameModel(currentFrame()); });
  connect(m_visibleCheck, &QCheckBox::toggled, ref, &Ref3DScene::setVisible);
  connect(m_gridCheck, &QCheckBox::toggled, ref, &Ref3DScene::setShowGrid);
  connect(m_autoKeyCheck, &QCheckBox::toggled, ref, &Ref3DScene::setAutoKey);
  connect(m_opacitySlider, &QSlider::valueChanged, ref,
          &Ref3DScene::setOpacity);
  connect(m_shadingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          [ref](int i) { ref->setShading(Ref3DScene::Shading(i)); });
  connect(m_easingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [ref](int i) { ref->setEasing(Ref3DScene::Easing(i)); });
  connect(m_focalField, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &Ref3DPanel::onCameraFieldChanged);
  connect(m_rollField, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &Ref3DPanel::onCameraFieldChanged);
  connect(m_setKeyButton, &QPushButton::clicked, this,
          [ref]() { ref->setKey(currentFrame()); });
  connect(m_removeKeyButton, &QPushButton::clicked, this,
          [ref]() { ref->removeKey(currentFrame()); });
  connect(m_prevKeyButton, &QPushButton::clicked, this,
          &Ref3DPanel::onPrevKey);
  connect(m_nextKeyButton, &QPushButton::clicked, this,
          &Ref3DPanel::onNextKey);

  connect(ref, &Ref3DScene::changed, this, &Ref3DPanel::updateControls);
  connect(TApp::instance()->getCurrentFrame(), SIGNAL(frameSwitched()), this,
          SLOT(updateControls()));
  connect(TApp::instance()->getCurrentScene(), SIGNAL(sceneSwitched()), this,
          SLOT(updateControls()));
  connect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameSwitched,
          m_viewport, [this]() { m_viewport->update(); });

  updateControls();
}

//-----------------------------------------------------------------------------

void Ref3DPanel::updateControls() {
  Ref3DScene *ref = Ref3DScene::instance();
  int frame       = currentFrame();
  Ref3DCamera cam = ref->currentCamera(frame);
  bool hasModel   = ref->hasModel();

  QList<QWidget *> fields = {m_visibleCheck, m_gridCheck,     m_autoKeyCheck,
                             m_focalField,   m_rollField,     m_opacitySlider,
                             m_shadingCombo, m_easingCombo};
  for (QWidget *w : fields) w->blockSignals(true);

  m_visibleCheck->setChecked(ref->isVisible());
  m_gridCheck->setChecked(ref->showGrid());
  m_autoKeyCheck->setChecked(ref->autoKey());
  m_focalField->setValue(int(std::round(cam.m_focal)));
  m_rollField->setValue(cam.m_roll);
  m_opacitySlider->setValue(ref->opacity());
  m_shadingCombo->setCurrentIndex(int(ref->shading()));
  m_easingCombo->setCurrentIndex(int(ref->easing()));

  for (QWidget *w : fields) w->blockSignals(false);

  m_modelLabel->setText(hasModel ? QFileInfo(ref->modelPath()).fileName()
                                 : tr("No model"));
  m_modelLabel->setToolTip(ref->modelPath());
  m_clearButton->setEnabled(hasModel);
  m_frameButton->setEnabled(hasModel);
  for (QWidget *w : {(QWidget *)m_focalField, (QWidget *)m_rollField,
                     (QWidget *)m_setKeyButton})
    w->setEnabled(hasModel);

  bool isKey = ref->isKey(frame);
  m_frameLabel->setText(tr("Frame %1").arg(frame + 1) +
                        (isKey ? QString::fromUtf8(" \xe2\x97\x86") : ""));
  m_removeKeyButton->setEnabled(isKey);
  m_prevKeyButton->setEnabled(ref->previousKey(frame) >= 0);
  m_nextKeyButton->setEnabled(ref->nextKey(frame) >= 0);

  QStringList keys;
  for (const auto &k : ref->keys()) keys << QString::number(k.first + 1);
  m_keysLabel->setText(keys.isEmpty()
                           ? tr("No camera keys: the camera is the same on "
                                "every frame.")
                           : tr("Camera keys on frames: %1").arg(keys.join(", ")));
}

void Ref3DPanel::onImport() {
  QSettings settings;
  QString lastDir = settings.value("Ref3D/LastDir").toString();
  QString path    = QFileDialog::getOpenFileName(
      this, tr("Import 3D Model"), lastDir,
      tr("3D Models (*.glb *.gltf *.obj)"));
  if (path.isEmpty()) return;
  settings.setValue("Ref3D/LastDir", QFileInfo(path).absolutePath());

  QString error;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  bool ok = Ref3DScene::instance()->loadModel(path, error);
  QApplication::restoreOverrideCursor();
  if (!ok)
    QMessageBox::warning(this, tr("Import 3D Model"),
                         tr("The model could not be imported.\n%1").arg(error));
}

void Ref3DPanel::onCameraFieldChanged() {
  Ref3DScene *ref = Ref3DScene::instance();
  int frame       = currentFrame();
  Ref3DCamera cam = ref->currentCamera(frame);
  cam.m_focal     = m_focalField->value();
  cam.m_roll      = m_rollField->value();
  ref->setFreeCamera(cam, frame);
}

void Ref3DPanel::onPrevKey() {
  int f = Ref3DScene::instance()->previousKey(currentFrame());
  if (f >= 0) TApp::instance()->getCurrentFrame()->setFrame(f);
}

void Ref3DPanel::onNextKey() {
  int f = Ref3DScene::instance()->nextKey(currentFrame());
  if (f >= 0) TApp::instance()->getCurrentFrame()->setFrame(f);
}

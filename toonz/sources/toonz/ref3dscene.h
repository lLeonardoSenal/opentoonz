#pragma once

#ifndef REF3DSCENE_H
#define REF3DSCENE_H

#include <QObject>
#include <QString>
#include <QVector3D>
#include <QMatrix4x4>

#include <map>
#include <memory>
#include <vector>

//=============================================================================
// 3D Reference
//
// A 3D model (exported from Blender as .glb/.gltf or .obj) shown behind the
// drawings in the viewer, seen through a camera that can be keyframed over
// the scene frames. It is meant as a perspective / volume reference for 2D
// animation: it never renders and it is not part of the xsheet.
//
// The state is saved next to the scene file as "<scene>.tnz.ref3d.json".
//=============================================================================

//! Triangle soup ready to be drawn with client-side vertex arrays.
struct Ref3DModel {
  std::vector<float> m_positions;  //!< xyz per vertex
  std::vector<float> m_normals;    //!< xyz per vertex
  std::vector<float> m_colors;     //!< rgb per vertex
  QVector3D m_bboxMin, m_bboxMax;

  int vertexCount() const { return (int)m_positions.size() / 3; }
  QVector3D center() const { return 0.5f * (m_bboxMin + m_bboxMax); }
  float radius() const;

  //! Loads .glb, .gltf or .obj. Returns false and fills \b error on failure.
  static std::shared_ptr<Ref3DModel> load(const QString &path,
                                          QString &error);
};

//-----------------------------------------------------------------------------

//! Orbit camera around a target point. Angles are in degrees, the lens in
//! millimeters on a 36mm-wide sensor (same convention as Blender).
struct Ref3DCamera {
  QVector3D m_target;
  double m_yaw = 30.0, m_pitch = 15.0, m_distance = 10.0;
  double m_focal = 35.0, m_roll = 0.0;

  QVector3D eye() const;
  QMatrix4x4 viewMatrix() const;
  QMatrix4x4 projectionMatrix(double aspect, double sceneRadius) const;

  static Ref3DCamera interpolate(const Ref3DCamera &a, const Ref3DCamera &b,
                                 double t);
};

//-----------------------------------------------------------------------------

class Ref3DScene final : public QObject {
  Q_OBJECT

public:
  enum Shading { Solid = 0, Wireframe, SolidWire };
  enum Easing { Linear = 0, Smooth };

private:
  std::shared_ptr<Ref3DModel> m_model;
  QString m_modelPath;

  std::map<int, Ref3DCamera> m_keys;  //!< frame (0-based row) -> camera
  Ref3DCamera m_freeCamera;  //!< camera being navigated, not yet keyed
  bool m_freeCameraDirty = false;

  bool m_visible = true, m_showGrid = true, m_autoKey = false;
  int m_opacity  = 60;  //!< 0-100
  Shading m_shading = Solid;
  Easing m_easing   = Smooth;

  bool m_loading = false;  //!< true while reading the sidecar file

  Ref3DScene();

public:
  static Ref3DScene *instance();

  bool hasModel() const { return m_model && m_model->vertexCount() > 0; }
  const Ref3DModel *model() const { return m_model.get(); }
  QString modelPath() const { return m_modelPath; }
  bool loadModel(const QString &path, QString &error);
  void clearModel();

  //! Camera shown at \b frame: the free camera while the user is
  //! navigating, otherwise the keyframed one.
  Ref3DCamera currentCamera(int frame) const;
  Ref3DCamera keyedCamera(int frame) const;
  void setFreeCamera(const Ref3DCamera &cam, int frame);
  void frameModel(int frame);

  const std::map<int, Ref3DCamera> &keys() const { return m_keys; }
  bool isKey(int frame) const { return m_keys.count(frame) > 0; }
  void setKey(int frame);
  void removeKey(int frame);
  int previousKey(int frame) const;  //!< -1 if none
  int nextKey(int frame) const;      //!< -1 if none

  bool isVisible() const { return m_visible; }
  void setVisible(bool on);
  bool showGrid() const { return m_showGrid; }
  void setShowGrid(bool on);
  bool autoKey() const { return m_autoKey; }
  void setAutoKey(bool on);
  int opacity() const { return m_opacity; }
  void setOpacity(int value);
  Shading shading() const { return m_shading; }
  void setShading(Shading s);
  Easing easing() const { return m_easing; }
  void setEasing(Easing e);

  //! Draws the reference in the current OpenGL context, into a viewport of
  //! the given aspect ratio. Uses the fixed pipeline and client-side arrays,
  //! so it works in any context without per-context resources. Clears the
  //! depth buffer (and the color to transparent if \b clearTarget); all
  //! other GL state is restored.
  void draw(const Ref3DCamera &cam, double aspect,
            bool clearTarget = false) const;

signals:
  void changed();

private slots:
  void onSceneSwitched();
  void onSceneNameChanged();
  void onFrameSwitched();

private:
  void notify(bool save = true);
  QString sidecarPath() const;
  void save() const;
  void loadSidecar();
  void reset();
  int currentFrame() const;
};

#endif  // REF3DSCENE_H

#include "ref3dscene.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"

// TnzCore includes
#include "tgl.h"

#include "tapp.h"

// Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QMatrix3x3>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

//=============================================================================
// Model loading
//-----------------------------------------------------------------------------

namespace {

const QVector3D DefaultModelColor(0.80f, 0.80f, 0.80f);

void pushVertex(Ref3DModel &m, const QVector3D &p, const QVector3D &n,
                const QVector3D &c) {
  m.m_positions.insert(m.m_positions.end(), {p.x(), p.y(), p.z()});
  m.m_normals.insert(m.m_normals.end(), {n.x(), n.y(), n.z()});
  m.m_colors.insert(m.m_colors.end(), {c.x(), c.y(), c.z()});
}

QVector3D faceNormal(const QVector3D &a, const QVector3D &b,
                     const QVector3D &c) {
  QVector3D n = QVector3D::crossProduct(b - a, c - a);
  return n.isNull() ? QVector3D(0, 1, 0) : n.normalized();
}

void computeBBox(Ref3DModel &m) {
  int n = m.vertexCount();
  if (n == 0) return;
  QVector3D lo(m.m_positions[0], m.m_positions[1], m.m_positions[2]), hi = lo;
  for (int i = 1; i < n; ++i) {
    const float *p = &m.m_positions[3 * i];
    lo = QVector3D(std::min(lo.x(), p[0]), std::min(lo.y(), p[1]),
                   std::min(lo.z(), p[2]));
    hi = QVector3D(std::max(hi.x(), p[0]), std::max(hi.y(), p[1]),
                   std::max(hi.z(), p[2]));
  }
  m.m_bboxMin = lo;
  m.m_bboxMax = hi;
}

//-----------------------------------------------------------------------------
// glTF / GLB (via cgltf). Files are read through QFile so that paths with
// non-ASCII characters work on Windows.

cgltf_result readFileUtf8(const cgltf_memory_options *, const cgltf_file_options *,
                          const char *path, cgltf_size *size, void **data) {
  QFile file(QString::fromUtf8(path));
  if (!file.open(QIODevice::ReadOnly)) return cgltf_result_file_not_found;
  QByteArray bytes = file.readAll();
  void *buffer     = std::malloc(bytes.size() > 0 ? bytes.size() : 1);
  if (!buffer) return cgltf_result_out_of_memory;
  std::memcpy(buffer, bytes.constData(), bytes.size());
  *size = bytes.size();
  *data = buffer;
  return cgltf_result_success;
}

void releaseFile(const cgltf_memory_options *, const cgltf_file_options *,
                 void *data) {
  std::free(data);
}

void addGltfNode(Ref3DModel &m, const cgltf_node *node) {
  if (node->mesh) {
    float w[16];
    cgltf_node_transform_world(node, w);
    QMatrix4x4 world = QMatrix4x4(w).transposed();  // cgltf is column-major
    QMatrix3x3 nm    = world.normalMatrix();

    for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p) {
      const cgltf_primitive &prim = node->mesh->primitives[p];
      if (prim.type != cgltf_primitive_type_triangles) continue;

      const cgltf_accessor *posAcc = nullptr, *nrmAcc = nullptr;
      for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
        if (prim.attributes[a].type == cgltf_attribute_type_position)
          posAcc = prim.attributes[a].data;
        else if (prim.attributes[a].type == cgltf_attribute_type_normal)
          nrmAcc = prim.attributes[a].data;
      }
      if (!posAcc) continue;

      QVector3D color = DefaultModelColor;
      if (prim.material && prim.material->has_pbr_metallic_roughness) {
        const float *f =
            prim.material->pbr_metallic_roughness.base_color_factor;
        color = QVector3D(f[0], f[1], f[2]);
      }

      cgltf_size count =
          prim.indices ? prim.indices->count : posAcc->count;
      count -= count % 3;
      for (cgltf_size i = 0; i < count; i += 3) {
        QVector3D pos[3], nrm[3];
        bool hasNormals = nrmAcc != nullptr;
        for (int k = 0; k < 3; ++k) {
          cgltf_size idx =
              prim.indices ? cgltf_accessor_read_index(prim.indices, i + k)
                           : i + k;
          float v[3] = {0, 0, 0};
          cgltf_accessor_read_float(posAcc, idx, v, 3);
          pos[k] = world.map(QVector3D(v[0], v[1], v[2]));
          if (hasNormals) {
            float n[3] = {0, 1, 0};
            cgltf_accessor_read_float(nrmAcc, idx, n, 3);
            const float *d = nm.constData();  // column-major 3x3
            QVector3D tn(d[0] * n[0] + d[3] * n[1] + d[6] * n[2],
                         d[1] * n[0] + d[4] * n[1] + d[7] * n[2],
                         d[2] * n[0] + d[5] * n[1] + d[8] * n[2]);
            nrm[k] = tn.isNull() ? QVector3D(0, 1, 0) : tn.normalized();
          }
        }
        if (!hasNormals)
          nrm[0] = nrm[1] = nrm[2] = faceNormal(pos[0], pos[1], pos[2]);
        for (int k = 0; k < 3; ++k) pushVertex(m, pos[k], nrm[k], color);
      }
    }
  }
  for (cgltf_size c = 0; c < node->children_count; ++c)
    addGltfNode(m, node->children[c]);
}

bool loadGltf(Ref3DModel &m, const QString &path, QString &error) {
  cgltf_options options;
  std::memset(&options, 0, sizeof(options));
  options.file.read    = readFileUtf8;
  options.file.release = releaseFile;

  QByteArray utf8Path = path.toUtf8();
  cgltf_data *data    = nullptr;
  cgltf_result res    = cgltf_parse_file(&options, utf8Path.constData(), &data);
  if (res == cgltf_result_success)
    res = cgltf_load_buffers(&options, data, utf8Path.constData());
  if (res != cgltf_result_success) {
    error =
        QObject::tr("Could not read the glTF file (error %1).").arg(int(res));
    if (data) cgltf_free(data);
    return false;
  }

  const cgltf_scene *scene =
      data->scene ? data->scene
                  : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
  if (scene) {
    for (cgltf_size i = 0; i < scene->nodes_count; ++i)
      addGltfNode(m, scene->nodes[i]);
  } else {
    for (cgltf_size i = 0; i < data->nodes_count; ++i)
      if (!data->nodes[i].parent) addGltfNode(m, &data->nodes[i]);
  }
  cgltf_free(data);
  return true;
}

//-----------------------------------------------------------------------------
// Wavefront OBJ (with optional .mtl diffuse colors)

std::map<QString, QVector3D> readMtl(const QString &path) {
  std::map<QString, QVector3D> colors;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return colors;
  QTextStream in(&file);
  QString current;
  while (!in.atEnd()) {
    QStringList t = in.readLine().simplified().split(' ');
    if (t.size() >= 2 && t[0] == "newmtl")
      current = t.mid(1).join(' ');
    else if (t.size() >= 4 && t[0] == "Kd" && !current.isEmpty())
      colors[current] =
          QVector3D(t[1].toFloat(), t[2].toFloat(), t[3].toFloat());
  }
  return colors;
}

bool loadObj(Ref3DModel &m, const QString &path, QString &error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    error = QObject::tr("Could not open the file.");
    return false;
  }
  std::vector<QVector3D> v, vn;
  std::map<QString, QVector3D> materials;
  QVector3D color = DefaultModelColor;
  QDir dir        = QFileInfo(path).absoluteDir();

  // resolves an OBJ index (1-based, negative = relative to the end)
  auto resolve = [](int i, size_t size) -> int {
    if (i > 0) return i - 1;
    if (i < 0) return (int)size + i;
    return -1;
  };

  QTextStream in(&file);
  while (!in.atEnd()) {
    QStringList t = in.readLine().simplified().split(' ');
    if (t.isEmpty()) continue;
    const QString &kw = t[0];
    if (kw == "v" && t.size() >= 4)
      v.emplace_back(t[1].toFloat(), t[2].toFloat(), t[3].toFloat());
    else if (kw == "vn" && t.size() >= 4)
      vn.emplace_back(t[1].toFloat(), t[2].toFloat(), t[3].toFloat());
    else if (kw == "mtllib" && t.size() >= 2) {
      auto mtl = readMtl(dir.filePath(t.mid(1).join(' ')));
      materials.insert(mtl.begin(), mtl.end());
    } else if (kw == "usemtl" && t.size() >= 2) {
      auto it = materials.find(t.mid(1).join(' '));
      color   = it != materials.end() ? it->second : DefaultModelColor;
    } else if (kw == "f" && t.size() >= 4) {
      std::vector<int> pi, ni;
      for (int k = 1; k < t.size(); ++k) {
        QStringList idx = t[k].split('/');
        pi.push_back(resolve(idx[0].toInt(), v.size()));
        ni.push_back(idx.size() >= 3 && !idx[2].isEmpty()
                         ? resolve(idx[2].toInt(), vn.size())
                         : -1);
      }
      // triangle fan
      for (size_t k = 1; k + 1 < pi.size(); ++k) {
        int tri[3] = {0, (int)k, (int)k + 1};
        bool ok    = true;
        for (int c : tri)
          if (pi[c] < 0 || pi[c] >= (int)v.size()) ok = false;
        if (!ok) continue;
        QVector3D fn = faceNormal(v[pi[0]], v[pi[k]], v[pi[k + 1]]);
        for (int c : tri) {
          QVector3D n = (ni[c] >= 0 && ni[c] < (int)vn.size())
                            ? vn[ni[c]].normalized()
                            : fn;
          pushVertex(m, v[pi[c]], n, color);
        }
      }
    }
  }
  return true;
}

}  // namespace

//-----------------------------------------------------------------------------

float Ref3DModel::radius() const {
  float r = 0.5f * (m_bboxMax - m_bboxMin).length();
  return r > 1e-4f ? r : 1.0f;
}

std::shared_ptr<Ref3DModel> Ref3DModel::load(const QString &path,
                                             QString &error) {
  auto model     = std::make_shared<Ref3DModel>();
  QString suffix = QFileInfo(path).suffix().toLower();
  bool ok;
  if (suffix == "glb" || suffix == "gltf")
    ok = loadGltf(*model, path, error);
  else if (suffix == "obj")
    ok = loadObj(*model, path, error);
  else {
    error = QObject::tr("Unsupported format. Use .glb, .gltf or .obj.");
    return nullptr;
  }
  if (!ok) return nullptr;
  if (model->vertexCount() == 0) {
    error = QObject::tr("The file does not contain any triangle mesh.");
    return nullptr;
  }
  computeBBox(*model);
  return model;
}

//=============================================================================
// Camera
//-----------------------------------------------------------------------------

QVector3D Ref3DCamera::eye() const {
  double y = qDegreesToRadians(m_yaw), p = qDegreesToRadians(m_pitch);
  return m_target + float(m_distance) * QVector3D(std::cos(p) * std::sin(y),
                                                  std::sin(p),
                                                  std::cos(p) * std::cos(y));
}

QMatrix4x4 Ref3DCamera::viewMatrix() const {
  QVector3D e = eye();
  QVector3D f = (m_target - e).normalized();
  QVector3D r = QVector3D::crossProduct(f, QVector3D(0, 1, 0));
  if (r.lengthSquared() < 1e-8f) r = QVector3D(1, 0, 0);
  r.normalize();
  QVector3D up = QVector3D::crossProduct(r, f);
  if (m_roll != 0.0) {
    QMatrix4x4 rot;
    rot.rotate(float(m_roll), f);
    up = rot.mapVector(up);
  }
  QMatrix4x4 view;
  view.lookAt(e, m_target, up);
  return view;
}

QMatrix4x4 Ref3DCamera::projectionMatrix(double aspect,
                                         double sceneRadius) const {
  // Blender's "Auto" sensor fit: the 36mm sensor spans the larger side
  double t = 18.0 / std::max(1.0, m_focal);
  double tx, ty;
  if (aspect >= 1.0)
    tx = t, ty = t / aspect;
  else
    ty = t, tx = t * aspect;
  double zNear = std::max({m_distance * 0.01, sceneRadius * 0.001, 1e-4});
  double zFar  = (m_distance + sceneRadius * 4.0) * 4.0;
  QMatrix4x4 proj;
  proj.frustum(-tx * zNear, tx * zNear, -ty * zNear, ty * zNear, zNear, zFar);
  return proj;
}

Ref3DCamera Ref3DCamera::interpolate(const Ref3DCamera &a,
                                     const Ref3DCamera &b, double t) {
  auto lerp = [t](double x, double y) { return x + (y - x) * t; };
  Ref3DCamera c;
  c.m_target   = a.m_target + float(t) * (b.m_target - a.m_target);
  c.m_yaw      = lerp(a.m_yaw, b.m_yaw);
  c.m_pitch    = lerp(a.m_pitch, b.m_pitch);
  c.m_focal    = lerp(a.m_focal, b.m_focal);
  c.m_roll     = lerp(a.m_roll, b.m_roll);
  // zoom feels linear in log space
  c.m_distance = std::exp(lerp(std::log(std::max(a.m_distance, 1e-6)),
                               std::log(std::max(b.m_distance, 1e-6))));
  return c;
}

//=============================================================================
// Ref3DScene
//-----------------------------------------------------------------------------

namespace {
QTimer *saveTimer = nullptr;
}

Ref3DScene::Ref3DScene() {
  TApp *app = TApp::instance();
  connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
          SLOT(onSceneSwitched()));
  connect(app->getCurrentScene(), SIGNAL(nameSceneChanged()), this,
          SLOT(onSceneNameChanged()));
  connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
          SLOT(onFrameSwitched()));

  // writing the sidecar on every mouse move while navigating is wasteful
  saveTimer = new QTimer(this);
  saveTimer->setSingleShot(true);
  saveTimer->setInterval(500);
  connect(saveTimer, &QTimer::timeout, this, [this]() { save(); });

  loadSidecar();
}

Ref3DScene *Ref3DScene::instance() {
  static Ref3DScene *_instance = new Ref3DScene();
  return _instance;
}

int Ref3DScene::currentFrame() const {
  return TApp::instance()->getCurrentFrame()->getFrame();
}

void Ref3DScene::notify(bool doSave) {
  if (doSave && !m_loading) saveTimer->start();
  emit changed();
}

//-----------------------------------------------------------------------------

bool Ref3DScene::loadModel(const QString &path, QString &error) {
  std::shared_ptr<Ref3DModel> model = Ref3DModel::load(path, error);
  if (!model) return false;
  bool first  = !m_model;
  m_model     = model;
  m_modelPath = QFileInfo(path).absoluteFilePath();
  if (first && m_keys.empty()) frameModel(currentFrame());
  notify();
  return true;
}

void Ref3DScene::clearModel() {
  m_model.reset();
  m_modelPath.clear();
  notify();
}

//-----------------------------------------------------------------------------

Ref3DCamera Ref3DScene::keyedCamera(int frame) const {
  if (m_keys.empty()) return m_freeCamera;
  auto next = m_keys.lower_bound(frame);
  if (next == m_keys.end()) return std::prev(next)->second;
  if (next->first == frame || next == m_keys.begin()) return next->second;
  auto prev = std::prev(next);
  double t  = double(frame - prev->first) / double(next->first - prev->first);
  if (m_easing == Smooth) t = t * t * (3.0 - 2.0 * t);
  return Ref3DCamera::interpolate(prev->second, next->second, t);
}

Ref3DCamera Ref3DScene::currentCamera(int frame) const {
  return m_freeCameraDirty ? m_freeCamera : keyedCamera(frame);
}

void Ref3DScene::setFreeCamera(const Ref3DCamera &cam, int frame) {
  m_freeCamera = cam;
  if (m_autoKey || m_keys.empty()) {
    // with no keys the free camera is the camera of every frame;
    // with auto-key every move is recorded on the current frame
    if (m_autoKey) m_keys[frame] = cam;
    m_freeCameraDirty = false;
  } else
    m_freeCameraDirty = true;
  notify();
}

void Ref3DScene::frameModel(int frame) {
  Ref3DCamera cam = currentCamera(frame);
  if (hasModel()) {
    double halfFov = std::atan(18.0 / std::max(1.0, cam.m_focal));
    cam.m_target   = m_model->center();
    cam.m_distance = m_model->radius() / std::sin(halfFov) * 1.1;
  }
  setFreeCamera(cam, frame);
}

void Ref3DScene::setKey(int frame) {
  m_keys[frame]     = currentCamera(frame);
  m_freeCameraDirty = false;
  notify();
}

void Ref3DScene::removeKey(int frame) {
  if (!m_keys.count(frame)) return;
  // keep looking through the same camera when the last key goes away
  Ref3DCamera cam = currentCamera(frame);
  m_keys.erase(frame);
  if (m_keys.empty()) {
    m_freeCamera      = cam;
    m_freeCameraDirty = false;
  }
  notify();
}

int Ref3DScene::previousKey(int frame) const {
  auto it = m_keys.lower_bound(frame);
  return it == m_keys.begin() ? -1 : std::prev(it)->first;
}

int Ref3DScene::nextKey(int frame) const {
  auto it = m_keys.upper_bound(frame);
  return it == m_keys.end() ? -1 : it->first;
}

//-----------------------------------------------------------------------------

void Ref3DScene::setVisible(bool on) {
  if (m_visible == on) return;
  m_visible = on;
  notify();
}

void Ref3DScene::setShowGrid(bool on) {
  if (m_showGrid == on) return;
  m_showGrid = on;
  notify();
}

void Ref3DScene::setAutoKey(bool on) {
  if (m_autoKey == on) return;
  m_autoKey = on;
  notify();
}

void Ref3DScene::setOpacity(int value) {
  value = std::max(0, std::min(100, value));
  if (m_opacity == value) return;
  m_opacity = value;
  notify();
}

void Ref3DScene::setShading(Shading s) {
  if (m_shading == s) return;
  m_shading = s;
  notify();
}

void Ref3DScene::setEasing(Easing e) {
  if (m_easing == e) return;
  m_easing = e;
  notify();
}

//-----------------------------------------------------------------------------

void Ref3DScene::onFrameSwitched() {
  // moving to another frame drops the camera that was not keyed
  if (m_freeCameraDirty) {
    m_freeCameraDirty = false;
    notify(false);
  } else if (m_keys.size() > 1)
    emit changed();
}

void Ref3DScene::onSceneSwitched() {
  saveTimer->stop();
  reset();
  loadSidecar();
  emit changed();
}

void Ref3DScene::onSceneNameChanged() {
  // the scene was saved under a (new) name: write the sidecar next to it
  save();
}

void Ref3DScene::reset() {
  m_model.reset();
  m_modelPath.clear();
  m_keys.clear();
  m_freeCamera      = Ref3DCamera();
  m_freeCameraDirty = false;
  m_visible = m_showGrid = true;
  m_autoKey              = false;
  m_opacity              = 60;
  m_shading              = Solid;
  m_easing               = Smooth;
}

//-----------------------------------------------------------------------------
// Sidecar file

namespace {

QJsonObject cameraToJson(const Ref3DCamera &c) {
  QJsonObject o;
  o["target"] =
      QJsonArray({c.m_target.x(), c.m_target.y(), c.m_target.z()});
  o["yaw"]      = c.m_yaw;
  o["pitch"]    = c.m_pitch;
  o["distance"] = c.m_distance;
  o["focal"]    = c.m_focal;
  o["roll"]     = c.m_roll;
  return o;
}

Ref3DCamera cameraFromJson(const QJsonObject &o) {
  Ref3DCamera c;
  QJsonArray t = o["target"].toArray();
  if (t.size() == 3)
    c.m_target = QVector3D(t[0].toDouble(), t[1].toDouble(), t[2].toDouble());
  c.m_yaw      = o["yaw"].toDouble(c.m_yaw);
  c.m_pitch    = o["pitch"].toDouble(c.m_pitch);
  c.m_distance = o["distance"].toDouble(c.m_distance);
  c.m_focal    = o["focal"].toDouble(c.m_focal);
  c.m_roll     = o["roll"].toDouble(c.m_roll);
  return c;
}

}  // namespace

QString Ref3DScene::sidecarPath() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return QString();
  TFilePath fp = scene->decodeFilePath(scene->getScenePath());
  return fp.getQString() + ".ref3d.json";
}

void Ref3DScene::save() const {
  QString path = sidecarPath();
  if (path.isEmpty()) return;

  if (!m_model && m_keys.empty()) {
    if (QFile::exists(path)) QFile::remove(path);
    return;
  }

  QJsonObject root;
  root["version"] = 1;
  if (!m_modelPath.isEmpty()) {
    root["model"] = m_modelPath;
    root["modelRelative"] =
        QFileInfo(path).absoluteDir().relativeFilePath(m_modelPath);
  }
  root["visible"]  = m_visible;
  root["grid"]     = m_showGrid;
  root["autoKey"]  = m_autoKey;
  root["opacity"]  = m_opacity;
  root["shading"]  = int(m_shading);
  root["easing"]   = int(m_easing);
  root["camera"]   = cameraToJson(m_freeCamera);
  QJsonArray keys;
  for (const auto &k : m_keys) {
    QJsonObject o = cameraToJson(k.second);
    o["frame"]    = k.first;
    keys.append(o);
  }
  root["keys"] = keys;

  QFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    file.write(QJsonDocument(root).toJson());
}

void Ref3DScene::loadSidecar() {
  QString path = sidecarPath();
  if (path.isEmpty()) return;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return;
  QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
  if (root.isEmpty()) return;

  m_loading    = true;
  m_visible    = root["visible"].toBool(true);
  m_showGrid   = root["grid"].toBool(true);
  m_autoKey    = root["autoKey"].toBool(false);
  m_opacity    = root["opacity"].toInt(60);
  m_shading    = Shading(root["shading"].toInt(Solid));
  m_easing     = Easing(root["easing"].toInt(Smooth));
  m_freeCamera = cameraFromJson(root["camera"].toObject());
  for (const QJsonValue &v : root["keys"].toArray()) {
    QJsonObject o              = v.toObject();
    m_keys[o["frame"].toInt()] = cameraFromJson(o);
  }

  // prefer the path relative to the scene, so that moving the project
  // folder together with the model keeps working
  QString rel = root["modelRelative"].toString();
  QString abs = root["model"].toString();
  QString candidate =
      rel.isEmpty() ? abs : QFileInfo(path).absoluteDir().filePath(rel);
  if (!QFile::exists(candidate)) candidate = abs;
  if (!candidate.isEmpty() && QFile::exists(candidate)) {
    QString error;
    m_model = Ref3DModel::load(candidate, error);
    if (m_model) m_modelPath = QFileInfo(candidate).absoluteFilePath();
  }
  m_loading = false;
}

//=============================================================================
// Drawing
//-----------------------------------------------------------------------------

namespace {

void drawGround(const Ref3DModel &m) {
  // grid on the floor of the model, to read the perspective
  float r    = m.radius();
  float step = std::pow(10.0f, std::floor(std::log10(r * 0.5f)));
  if (r / step > 20) step *= 5;
  else if (r / step > 8) step *= 2;
  int n      = std::max(1, int(std::ceil(r * 3.0f / step)));
  float ext  = n * step;
  float y    = m.m_bboxMin.y();
  QVector3D c = m.center();
  float cx = std::round(c.x() / step) * step, cz = std::round(c.z() / step) * step;

  glBegin(GL_LINES);
  for (int i = -n; i <= n; ++i) {
    float a = (i == 0) ? 0.45f : 0.62f;
    glColor3f(a, a, a);
    glVertex3f(cx + i * step, y, cz - ext);
    glVertex3f(cx + i * step, y, cz + ext);
    glVertex3f(cx - ext, y, cz + i * step);
    glVertex3f(cx + ext, y, cz + i * step);
  }
  glEnd();
}

}  // namespace

void Ref3DScene::draw(const Ref3DCamera &cam, double aspect,
                      bool clearTarget) const {
  if (!hasModel()) return;
  const Ref3DModel &m = *m_model;

  // The host context may have a shader program, buffers or a vertex array
  // object bound (Qt and some viewers use them): the fixed pipeline with
  // client-side arrays needs all of them unbound.
  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if (!ctx) return;
  QOpenGLFunctions *f       = ctx->functions();
  QOpenGLExtraFunctions *xf = ctx->format().majorVersion() >= 3
                                  ? ctx->extraFunctions()
                                  : nullptr;
  GLint prevProgram = 0, prevArrayBuffer = 0, prevElementBuffer = 0,
        prevVao = 0, prevActiveTexture = GL_TEXTURE0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevElementBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
  if (xf) glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
  f->glUseProgram(0);
  f->glBindBuffer(GL_ARRAY_BUFFER, 0);
  f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  f->glActiveTexture(GL_TEXTURE0);
  if (xf) xf->glBindVertexArray(0);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_COLOR_LOGIC_OP);
  glDisable(GL_FOG);
  glDisable(GL_LINE_STIPPLE);
  glDisable(GL_POLYGON_STIPPLE);
  for (int i = 0; i < 6; ++i) glDisable(GL_CLIP_PLANE0 + i);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glLineWidth(1.0f);

  // the depth buffer of the caller may hold anything: always start clean
  glClearDepth(1.0);
  if (clearTarget) {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  } else
    glClear(GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadMatrixf(cam.projectionMatrix(aspect, m.radius()).constData());
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // lights are fixed to the camera, like a key light on the shoulder
  const GLfloat keyPos[]  = {0.4f, 0.6f, 1.0f, 0.0f};
  const GLfloat keyDif[]  = {0.75f, 0.75f, 0.75f, 1.0f};
  const GLfloat fillPos[] = {-0.6f, -0.2f, 0.5f, 0.0f};
  const GLfloat fillDif[] = {0.25f, 0.25f, 0.28f, 1.0f};
  const GLfloat ambient[] = {0.30f, 0.30f, 0.30f, 1.0f};
  const GLfloat black[]   = {0, 0, 0, 1};
  glLightfv(GL_LIGHT0, GL_POSITION, keyPos);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, keyDif);
  glLightfv(GL_LIGHT0, GL_SPECULAR, black);
  glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDif);
  glLightfv(GL_LIGHT1, GL_SPECULAR, black);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

  glLoadMatrixf(cam.viewMatrix().constData());

  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  if (m_showGrid) drawGround(m);

  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, m.m_positions.data());

  if (m_shading != Wireframe) {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glNormalPointer(GL_FLOAT, 0, m.m_normals.data());
    glColorPointer(3, GL_FLOAT, 0, m.m_colors.data());
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, m.vertexCount());
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_LIGHTING);
  }
  if (m_shading != Solid) {
    if (m_shading == Wireframe)
      glColor3f(0.20f, 0.22f, 0.28f);
    else
      glColor3f(0.12f, 0.12f, 0.12f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawArrays(GL_TRIANGLES, 0, m.vertexCount());
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glPopClientAttrib();
  glPopAttrib();

  if (xf) xf->glBindVertexArray(prevVao);
  f->glActiveTexture(prevActiveTexture);
  f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevElementBuffer);
  f->glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
  f->glUseProgram(prevProgram);
}

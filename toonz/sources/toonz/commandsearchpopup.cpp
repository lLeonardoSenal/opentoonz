#include "commandsearchpopup.h"

#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const int CategoryRole = Qt::UserRole;
const int ShortcutRole = Qt::UserRole + 1;
const int EntryRole    = Qt::UserRole + 2;

// most recently run commands, newest first
QStringList recentIds;

//-----------------------------------------------------------------------------
// One row: category (dim) | command name | shortcut (dim, right aligned)

class CommandItemDelegate final : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QString name = opt.text;

    // The selection is painted here rather than by the style: some themes
    // give list selections a light background that the text cannot use.
    bool selected = opt.state & QStyle::State_Selected;
    bool enabled  = opt.state & QStyle::State_Enabled;
    QColor text   = opt.palette.color(
        enabled ? QPalette::Normal : QPalette::Disabled, QPalette::Text);
    if (selected) {
      QColor hl = opt.palette.color(QPalette::Highlight);
      if (hl.lightness() > 160) hl = QColor(0x3d, 0x8e, 0xe0);
      p->fillRect(opt.rect, hl);
      text = Qt::white;
    } else if (opt.state & QStyle::State_MouseOver)
      p->fillRect(opt.rect, QColor(255, 255, 255, 18));
    QColor dim = text;
    dim.setAlphaF(selected ? 0.8 : 0.55);

    QRect r = opt.rect.adjusted(8, 0, -8, 0);
    int categoryWidth = 96;
    p->save();
    p->setPen(dim);
    p->drawText(QRect(r.left(), r.top(), categoryWidth, r.height()),
                Qt::AlignVCenter | Qt::AlignLeft,
                index.data(CategoryRole).toString());
    QString shortcut = index.data(ShortcutRole).toString();
    int shortcutWidth = opt.fontMetrics.horizontalAdvance(shortcut);
    p->drawText(r, Qt::AlignVCenter | Qt::AlignRight, shortcut);
    p->setPen(text);
    QRect nameRect(r.left() + categoryWidth, r.top(),
                   r.width() - categoryWidth - shortcutWidth - 12, r.height());
    p->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                opt.fontMetrics.elidedText(name, Qt::ElideRight,
                                           nameRect.width()));
    p->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    return QSize(s.width(), std::max(s.height(), 24));
  }
};

}  // namespace

//=============================================================================

CommandSearchPopup::CommandSearchPopup(QWidget *parent)
    : QFrame(parent, Qt::Popup) {
  setObjectName("CommandSearchPopup");
  setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

  m_field = new QLineEdit(this);
  m_field->setObjectName("CommandSearchField");
  m_field->setPlaceholderText(
      tr("Type what you want to do: new level, export, onion skin..."));
  m_field->setClearButtonEnabled(true);
  m_field->installEventFilter(this);

  m_list = new QListWidget(this);
  m_list->setObjectName("CommandSearchList");
  m_list->setItemDelegate(new CommandItemDelegate(m_list));
  m_list->setUniformItemSizes(true);
  m_list->setFocusPolicy(Qt::NoFocus);

  QLabel *hint = new QLabel(
      tr("Enter: run   Up/Down: choose   Esc: close"), this);
  hint->setObjectName("CommandSearchHint");
  hint->setEnabled(false);

  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setMargin(6);
  lay->setSpacing(4);
  lay->addWidget(m_field);
  lay->addWidget(m_list, 1);
  lay->addWidget(hint);

  connect(m_field, &QLineEdit::textChanged, this,
          &CommandSearchPopup::refilter);
  connect(m_list, &QListWidget::itemActivated, this,
          &CommandSearchPopup::runCurrent);
  connect(m_list, &QListWidget::itemClicked, this,
          &CommandSearchPopup::runCurrent);
}

//-----------------------------------------------------------------------------

CommandSearchPopup *CommandSearchPopup::instance() {
  static CommandSearchPopup *_instance =
      new CommandSearchPopup(TApp::instance()->getMainWindow());
  return _instance;
}

//-----------------------------------------------------------------------------

void CommandSearchPopup::collectCommands() {
  static const std::pair<CommandType, const char *> types[] = {
      {MenuFileCommandType, QT_TR_NOOP("File")},
      {MenuEditCommandType, QT_TR_NOOP("Edit")},
      {MenuScanCleanupCommandType, QT_TR_NOOP("Cleanup")},
      {MenuLevelCommandType, QT_TR_NOOP("Level")},
      {MenuXsheetCommandType, QT_TR_NOOP("Xsheet")},
      {MenuCellsCommandType, QT_TR_NOOP("Cells")},
      {MenuPlayCommandType, QT_TR_NOOP("Play")},
      {MenuRenderCommandType, QT_TR_NOOP("Render")},
      {MenuViewCommandType, QT_TR_NOOP("View")},
      {MenuWindowsCommandType, QT_TR_NOOP("Windows")},
      {MenuHelpCommandType, QT_TR_NOOP("Help")},
      {ToolCommandType, QT_TR_NOOP("Tool")},
      {RoomCommandType, QT_TR_NOOP("Room")},
      {PlaybackCommandType, QT_TR_NOOP("Playback")},
      {ZoomCommandType, QT_TR_NOOP("Zoom")},
      {FillCommandType, QT_TR_NOOP("Fill")},
      {RightClickMenuCommandType, QT_TR_NOOP("Context")},
      {VisualizationButtonCommandType, QT_TR_NOOP("View")},
      {ToolModifierCommandType, QT_TR_NOOP("Tool Option")},
      {RGBACommandType, QT_TR_NOOP("Channel")},
      {BrushSizeCommandType, QT_TR_NOOP("Brush")},
      {MiscCommandType, QT_TR_NOOP("Misc")}};

  CommandManager *cm = CommandManager::instance();
  m_entries.clear();
  for (const auto &t : types) {
    std::vector<QAction *> actions;
    cm->getActions(t.first, actions);
    QString category = tr(t.second);
    for (QAction *action : actions) {
      if (!action || action->isSeparator()) continue;
      QString text = action->text();
      text.remove('&');
      text = text.trimmed();
      QString id = QString::fromStdString(cm->getIdFromAction(action));
      if (text.isEmpty() || id == MI_SearchCommands) continue;
      m_entries.push_back(
          {action, id, text, category, (text + " " + category).toLower()});
    }
  }
  std::sort(m_entries.begin(), m_entries.end(),
            [](const Entry &a, const Entry &b) {
              return a.m_text.localeAwareCompare(b.m_text) < 0;
            });
}

//-----------------------------------------------------------------------------

void CommandSearchPopup::popup() {
  collectCommands();

  // centered near the top of the main window, or of its screen when the
  // window is minimized or still being laid out
  QWidget *mainWindow = parentWidget();
  QRect area;
  if (mainWindow && mainWindow->isVisible() && !mainWindow->isMinimized() &&
      mainWindow->width() > 400)
    area = QRect(mainWindow->mapToGlobal(QPoint(0, 0)), mainWindow->size());
  else {
    QScreen *screen = mainWindow ? mainWindow->screen() : nullptr;
    if (!screen) screen = QGuiApplication::primaryScreen();
    area = screen->availableGeometry();
  }
  int w = std::max(360, std::min(560, area.width() - 40));
  int h = std::max(240, std::min(420, area.height() - 120));
  setGeometry(area.left() + (area.width() - w) / 2, area.top() + 72, w, h);

  m_field->clear();
  refilter();
  show();
  raise();
  activateWindow();
  m_field->setFocus();
}

//-----------------------------------------------------------------------------

void CommandSearchPopup::refilter() {
  QStringList tokens =
      m_field->text().toLower().split(' ', Qt::SkipEmptyParts);
  QString query = m_field->text().trimmed().toLower();

  struct Hit {
    const Entry *entry;
    int score;
  };
  std::vector<Hit> hits;
  for (const Entry &e : m_entries) {
    bool ok = true;
    for (const QString &tok : tokens)
      if (!e.m_haystack.contains(tok)) {
        ok = false;
        break;
      }
    if (!ok) continue;

    int score   = 3;
    int recent  = recentIds.indexOf(e.m_id);
    QString low = e.m_text.toLower();
    if (query.isEmpty())
      score = recent >= 0 ? 0 : 3;
    else if (low.startsWith(query))
      score = 0;
    else if (low.contains(" " + tokens.first()) ||
             low.startsWith(tokens.first()))
      score = 1;
    else
      score = 2;
    // recently used commands go first within the same score
    score = score * 100 + (recent >= 0 ? recent : 99);
    hits.push_back({&e, score});
  }
  std::stable_sort(hits.begin(), hits.end(),
                   [](const Hit &a, const Hit &b) { return a.score < b.score; });

  m_list->clear();
  const int maxRows = 300;
  for (int i = 0; i < (int)hits.size() && i < maxRows; ++i) {
    const Entry *e = hits[i].entry;
    QListWidgetItem *item = new QListWidgetItem(e->m_text, m_list);
    item->setData(CategoryRole, e->m_category);
    item->setData(ShortcutRole,
                  e->m_action->shortcut().toString(QKeySequence::NativeText));
    item->setData(EntryRole, QVariant::fromValue((void *)e->m_action));
    item->setToolTip(e->m_action->toolTip());
    if (!e->m_action->isEnabled()) item->setFlags(Qt::NoItemFlags);
  }
  for (int i = 0; i < m_list->count(); ++i)
    if (m_list->item(i)->flags() & Qt::ItemIsEnabled) {
      m_list->setCurrentRow(i);
      break;
    }
}

//-----------------------------------------------------------------------------

void CommandSearchPopup::moveSelection(int delta) {
  int n = m_list->count();
  if (n == 0) return;
  int row = m_list->currentRow();
  for (int i = 0; i < n; ++i) {
    row = (row + delta + n) % n;
    if (m_list->item(row)->flags() & Qt::ItemIsEnabled) {
      m_list->setCurrentRow(row);
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void CommandSearchPopup::runCurrent() {
  QListWidgetItem *item = m_list->currentItem();
  if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
  QAction *action = (QAction *)item->data(EntryRole).value<void *>();
  if (!action) return;

  QString id = QString::fromStdString(
      CommandManager::instance()->getIdFromAction(action));
  recentIds.removeAll(id);
  recentIds.prepend(id);
  while (recentIds.size() > 10) recentIds.removeLast();

  hide();
  // run once the popup is gone, so that dialogs opened by the command get
  // the focus and the command sees the main window as active
  QTimer::singleShot(0, action, [action]() { action->trigger(); });
}

//-----------------------------------------------------------------------------

bool CommandSearchPopup::eventFilter(QObject *obj, QEvent *e) {
  if (obj == m_field && e->type() == QEvent::KeyPress) {
    QKeyEvent *ke = static_cast<QKeyEvent *>(e);
    switch (ke->key()) {
    case Qt::Key_Down:
      moveSelection(1);
      return true;
    case Qt::Key_Up:
      moveSelection(-1);
      return true;
    case Qt::Key_PageDown:
      moveSelection(10);
      return true;
    case Qt::Key_PageUp:
      moveSelection(-10);
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      runCurrent();
      return true;
    case Qt::Key_Escape:
      hide();
      return true;
    default:
      break;
    }
  }
  return QFrame::eventFilter(obj, e);
}

//=============================================================================

class SearchCommandsHandler final : public MenuItemHandler {
public:
  SearchCommandsHandler() : MenuItemHandler(MI_SearchCommands) {}
  void execute() override { CommandSearchPopup::instance()->popup(); }
} searchCommandsHandler;

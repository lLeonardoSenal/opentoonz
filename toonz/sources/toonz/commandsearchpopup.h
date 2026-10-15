#pragma once

#ifndef COMMANDSEARCHPOPUP_H
#define COMMANDSEARCHPOPUP_H

#include <QFrame>
#include <QString>

#include <vector>

class QAction;
class QLineEdit;
class QListWidget;

//=============================================================================
// CommandSearchPopup
//   Lists every menu, tool and room command; typing filters them and Enter
//   runs the selected one. Opened with Help > Search Commands
//   (Ctrl+Shift+K), so that a command can be found without knowing which
//   menu it lives in.
//-----------------------------------------------------------------------------

class CommandSearchPopup final : public QFrame {
  Q_OBJECT

  struct Entry {
    QAction *m_action;
    QString m_id, m_text, m_category, m_haystack;
  };

  QLineEdit *m_field;
  QListWidget *m_list;
  std::vector<Entry> m_entries;

  CommandSearchPopup(QWidget *parent);

public:
  static CommandSearchPopup *instance();

  //! Collects the commands again, places the popup over the main window and
  //! shows it with an empty search field.
  void popup();

protected:
  bool eventFilter(QObject *obj, QEvent *e) override;

private:
  void collectCommands();
  void moveSelection(int delta);

private slots:
  void refilter();
  void runCurrent();
};

#endif  // COMMANDSEARCHPOPUP_H

#ifndef GOTOWIDGET_H
#define GOTOWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QListView>
#include <QCompleter>
#include "prefsdk/disassembler/disassemblerdefinition.h"

using namespace PrefSDK;

namespace Ui {
class GotoWidget;
}

class GotoWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit GotoWidget(QWidget *parent = 0);
        ~GotoWidget();
        void setDisassembler(DisassemblerDefinition* definition);
        QLineEdit *addressWidget();

    private slots:
        void gotoBlock();

    protected:
        virtual void hideEvent(QHideEvent* e);
        virtual void showEvent(QShowEvent* e);
        virtual void keyPressEvent(QKeyEvent* e);

    signals:
        void addressRequested(const PrefSDK::DataValue& dv);

    private:
        Ui::GotoWidget *ui;
        DisassemblerDefinition* _disassembler;
};

#endif // GOTOWIDGET_H

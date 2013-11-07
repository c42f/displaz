#ifndef HELP_DIALOG_H_INCLUDED
#define HELP_DIALOG_H_INCLUDED

#include <QtCore/QFile>
#include <QtGui/QDialog>
#include <QtGui/QTextEdit>
#include <QtGui/QGridLayout>

/// A very simple help dialog, which reads the text or generated html README
/// documentation from the installed doc directory.
class HelpDialog : public QDialog
{
    Q_OBJECT
    public:
        HelpDialog(QWidget* parent = 0)
            : QDialog(parent)
        {
            resize(700,900);
            setSizeGripEnabled(true);
            setWindowTitle("Displaz help");
            QGridLayout* gridLayout = new QGridLayout(this);
            gridLayout->setContentsMargins(2, 2, 2, 2);
            QTextEdit* textEdit = new QTextEdit(this);
            textEdit->setReadOnly(true);
            setHelpText(textEdit);
            gridLayout->addWidget(textEdit, 0, 0, 1, 1);
        }

    private:
        void setHelpText(QTextEdit* textEdit)
        {
            QFile docFile("doc:README.html");
            if (docFile.open(QIODevice::ReadOnly))
            {
                textEdit->setHtml(docFile.readAll());
                return;
            }
            // Look for text doc as backup
            docFile.setFileName("doc:README.rst");
            if (docFile.open(QIODevice::ReadOnly))
            {
                textEdit->setPlainText(docFile.readAll());
                return;
            }
            textEdit->setPlainText("Documentation not found");
        }
};


#endif // HELP_DIALOG_H_INCLUDED

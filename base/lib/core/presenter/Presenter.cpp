#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

#include <core/application/Application.hpp>
#include <core/application/OpenDocumentsFile.hpp>
#include <core/view/View.hpp>

#include <iscore/plugins/panel/PanelFactory.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelView.hpp>
#include <iscore/tools/exceptions/MissingCommand.hpp>

#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>

#include <core/undo/UndoControl.hpp>

#include <core/document/DocumentBackups.hpp>

#include "iscore_git_info.hpp"

#include <QFileDialog>
#include <QSaveFile>
#include <QMessageBox>
#include <QToolBar>
#include <QJsonDocument>

using namespace iscore;

Presenter::Presenter(View* view, QObject* arg_parent) :
    NamedObject {"Presenter", arg_parent},
    m_view {view},
    #ifdef __APPLE__
    m_menubar {new QMenuBar, this}
  #else
    m_menubar {view->menuBar(), this}
  #endif
{
    setupMenus();
    connect(m_view,		&View::insertActionIntoMenubar,
            &m_menubar, &MenubarManager::insertActionIntoMenubar);
    connect(m_view, &View::activeDocumentChanged,
            this,   &Presenter::setCurrentDocument);

    connect(m_view, &View::closeRequested,
            this,   &Presenter::closeDocument);
}

void Presenter::registerPluginControl(PluginControlInterface* ctrl)
{
    ctrl->setParent(this);

    ctrl->populateMenus(&m_menubar);
    m_toolbars += ctrl->makeToolbars();

    m_controls.push_back(ctrl);
}

void Presenter::registerPanel(PanelFactory* factory)
{
    auto view = factory->makeView(m_view);
    auto pres = factory->makePresenter(this, view);

    m_panelPresenters.push_back({pres, factory});

    m_view->setupPanelView(view);

    for(auto doc : m_documents)
        doc->setupNewPanel(factory);
}

void Presenter::registerDocumentDelegate(DocumentDelegateFactoryInterface* docpanel)
{
    m_availableDocuments.push_back(docpanel);
}

const std::vector<PluginControlInterface *> &Presenter::pluginControls() const
{
    return m_controls;
}

const std::vector<DocumentDelegateFactoryInterface *>& Presenter::availableDocuments() const
{
    return m_availableDocuments;
}

Document* Presenter::setupDocument(Document* doc)
{
    if(doc)
    {
        for(auto& panel : m_panelPresenters)
        {
            doc->setupNewPanel(panel.second);
        }

        m_documents.push_back(doc);
        m_view->addDocumentView(&doc->view());
        setCurrentDocument(doc);
    }
    else
    {
        setCurrentDocument(m_documents.empty() ? nullptr : m_documents.first());
    }

    return doc;
}

Document *Presenter::currentDocument() const
{
    return m_currentDocument;
}

void Presenter::setCurrentDocument(Document* doc)
{
    m_currentDocument = doc;

    for(auto& pair : m_panelPresenters)
    {
        if(doc)
            m_currentDocument->bindPanelPresenter(pair.first);
        else
            pair.first->setModel(nullptr);
    }

    for(auto& ctrl : m_controls)
    {
        emit ctrl->documentChanged();
    }
}

bool Presenter::closeDocument(Document* doc)
{
    // Warn the user if he might loose data
    if(!doc->commandStack().isAtSavedIndex())
    {
        QMessageBox msgBox;
        msgBox.setText(tr("The document has been modified."));
        msgBox.setInformativeText(tr("Do you want to save your changes?"));
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret)
        {
            case QMessageBox::Save:
                if(saveDocument(doc))
                    break;
                else
                    return false;
            case QMessageBox::Discard:
                // Do nothing
                break;
            case QMessageBox::Cancel:
                return false;
                break;
            default:
                break;
        }
    }

    // Close operation
    m_view->closeDocument(&doc->view());
    m_documents.removeOne(doc);

    setCurrentDocument(m_documents.size() > 0 ? m_documents.last() : nullptr);

    delete doc;
    return true;
}

bool Presenter::saveDocument(Document * doc)
{
    auto savename = doc->docFileName();

    if(savename == tr("Untitled"))
    {
        saveDocumentAs(doc);
    }
    else if (savename.size() != 0)
    {
        QSaveFile f{savename};
        f.open(QIODevice::WriteOnly);
        if(savename.indexOf(".scorebin") != -1)
            f.write(doc->saveAsByteArray());
        else
        {
            QJsonDocument json_doc;
            json_doc.setObject(doc->saveAsJson());

            f.write(json_doc.toJson());
        }
        f.commit();
    }

    return true;
}

bool Presenter::saveDocumentAs(Document * doc)
{
    QFileDialog d{m_view, tr("Save Document As")};
    QString binFilter{tr("Binary (*.scorebin)")};
    QString jsonFilter{tr("JSON (*.scorejson)")};
    QStringList filters;
    filters << binFilter
            << jsonFilter;

    d.setNameFilters(filters);
    d.setConfirmOverwrite(true);
    d.setFileMode(QFileDialog::AnyFile);
    d.setAcceptMode(QFileDialog::AcceptSave);

    if(d.exec())
    {
        QString savename = d.selectedFiles().first();
        auto suf = d.selectedNameFilter();

        if(!savename.isEmpty())
        {
            if(suf == binFilter)
            {
                if(!savename.contains(".scorebin"))
                    savename += ".scorebin";
            }
            else
            {
                if(!savename.contains(".scorejson"))
                    savename += ".scorejson";
            }

            QSaveFile f{savename};
            f.open(QIODevice::WriteOnly);
            doc->setDocFileName(savename);
            if(savename.indexOf(".scorebin") != -1)
                f.write(doc->saveAsByteArray());
            else
            {
                QJsonDocument json_doc;
                json_doc.setObject(doc->saveAsJson());

                f.write(json_doc.toJson());
            }
            f.commit();
        }
        return true;
    }
    return false;
}

Document* Presenter::loadDocument()
{
    Document* doc{};
    QString loadname = QFileDialog::getOpenFileName(m_view, tr("Open"), QString(), "*.scorebin *.scorejson");

    if(!loadname.isEmpty())
    {
        QFile f {loadname};
        if(f.open(QIODevice::ReadOnly))
        {
            if (loadname.indexOf(".scorebin") != -1)
            {
                doc = loadDocument(f.readAll(), m_availableDocuments.front());
            }
            else if (loadname.indexOf(".scorejson") != -1)
            {
                auto json = QJsonDocument::fromJson(f.readAll());
                doc = loadDocument(json.object(), m_availableDocuments.front());
            }
        }
    }
    m_currentDocument->setDocFileName(loadname);

    return doc;
}


void Presenter::restoreDocuments()
{
    for(const auto& backup : DocumentBackups::restorableDocuments())
    {
        restoreDocument(backup.first, backup.second, m_availableDocuments.front());
    }
}

iscore::SerializableCommand* Presenter::instantiateUndoCommand(
        const QString& parent_name,
        const QString& name,
        const QByteArray& data)
{
    for(auto& ccmd : m_controls)
    {
        if(ccmd->objectName() == parent_name)
        {
            return ccmd->instantiateUndoCommand(name, data);
        }
    }

#if defined(ISCORE_DEBUG)
    qDebug() << "ALERT: Command" << parent_name << "::" << name << "could not be instantiated.";
    ISCORE_ABORT;
#else
    throw MissingCommandException(parent_name, name);
#endif
    return nullptr;
}
#define xstr(s) str(s)
#define str(s) #s
void Presenter::setupMenus()
{
    ////// File //////
    auto newAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                                      FileMenuElement::New,
                                                      [&] () { newDocument(m_availableDocuments.front()); });

    newAct->setShortcut(QKeySequence::New);

    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                           FileMenuElement::Separator_Load);

    //// Save and load
    auto openAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                                       FileMenuElement::Load,
                                                       [this]() { loadDocument(); });
    openAct->setShortcut(QKeySequence::Open);

    auto saveAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                                       FileMenuElement::Save,
                                                       [this]() { saveDocument(currentDocument()); });
    saveAct->setShortcut(QKeySequence::Save);

    auto saveAsAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                                       FileMenuElement::SaveAs,
                                                       [this]() { saveDocumentAs(currentDocument()); });
    saveAsAct->setShortcut(QKeySequence::SaveAs);


    //	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
    //										FileMenuElement::SaveAs,
    //										notyet);

    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                           FileMenuElement::Separator_Export);

    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                           FileMenuElement::Separator_Quit);


    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Quit,
                                        [&] () {
        while(!m_documents.empty())
        {
            bool b = closeDocument(m_documents.last());
            if(!b)
                return;
        }

        qApp->quit();
    });

    ////// View //////
    m_menubar.addMenuIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                      ViewMenuElement::Windows);

    ////// Settings //////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
                                        SettingsMenuElement::Settings,
                                        std::bind(&SettingsView::exec,
                                                  qobject_cast<Application*> (parent())->settings()->view()));

    ////// About /////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::AboutMenu,
                                        AboutMenuElement::About, [] () {
        QMessageBox::about(nullptr,
                           tr("About i-score"),
                           tr("With love and sweat from the i-score team. \nVersion:\n")
                           + QString("%1.%2.%3-%4")
                           .arg(ISCORE_VERSION_MAJOR)
                           .arg(ISCORE_VERSION_MINOR)
                           .arg(ISCORE_VERSION_PATCH)
                           .arg(ISCORE_VERSION_EXTRA)
                           + tr("\n\nCommit: \n")
                           + QString(xstr(GIT_COMMIT))); });
}

View* Presenter::view() const
{
    return m_view;
}

const std::vector<iscore::PluginControlInterface*>& Presenter::controls() const
{
    return m_controls;
}


// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "diagnosticmanager.h"

#include "client.h"
#include "languageclienttr.h"

#include <coreplugin/editormanager/documentmodel.h>

#include <projectexplorer/project.h>

#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/textmark.h>
#include <texteditor/textstyles.h>

#include <utils/stringutils.h>
#include <utils/utilsicons.h>

#include <QAction>

using namespace LanguageServerProtocol;
using namespace Utils;
using namespace TextEditor;

namespace LanguageClient {

class TextMark : public TextEditor::TextMark
{
public:
    TextMark(TextDocument *doc, const Diagnostic &diag, const Client *client)
        : TextEditor::TextMark(doc, diag.range().start().line() + 1, {client->name(), client->id()})
    {
        setLineAnnotation(diag.message());
        setToolTip(diag.message());
        const bool isError
            = diag.severity().value_or(DiagnosticSeverity::Hint) == DiagnosticSeverity::Error;
        setColor(isError ? Theme::CodeModel_Error_TextMarkColor
                         : Theme::CodeModel_Warning_TextMarkColor);

        setIcon(isError ? Icons::CODEMODEL_ERROR.icon()
                        : Icons::CODEMODEL_WARNING.icon());
    }
};

struct VersionedDiagnostics
{
    std::optional<int> version;
    QList<LanguageServerProtocol::Diagnostic> diagnostics;
};

class Marks
{
public:
    ~Marks() { qDeleteAll(marks); }
    bool enabled = true;
    QList<TextEditor::TextMark *> marks;
};

class DiagnosticManager::DiagnosticManagerPrivate
{
public:
    DiagnosticManagerPrivate(Client *client)
        : m_client(client)
    {}

    QMap<Utils::FilePath, VersionedDiagnostics> m_diagnostics;
    QMap<Utils::FilePath, Marks> m_marks;
    Client *m_client;
    Utils::Id m_extraSelectionsId = TextEditorWidget::CodeWarningsSelection;
};

DiagnosticManager::DiagnosticManager(Client *client)
    : d(std::make_unique<DiagnosticManagerPrivate>(client))
{
}

DiagnosticManager::~DiagnosticManager()
{
    clearDiagnostics();
}

void DiagnosticManager::setDiagnostics(const FilePath &filePath,
                                       const QList<Diagnostic> &diagnostics,
                                       const std::optional<int> &version)
{
    hideDiagnostics(filePath);
    d->m_diagnostics[filePath] = {version, filteredDiagnostics(diagnostics)};
}

void DiagnosticManager::hideDiagnostics(const Utils::FilePath &filePath)
{
    if (auto doc = TextDocument::textDocumentForFilePath(filePath)) {
        for (BaseTextEditor *editor : BaseTextEditor::textEditorsForDocument(doc))
            editor->editorWidget()->setExtraSelections(d->m_extraSelectionsId, {});
    }
    d->m_marks.remove(filePath);
}

QList<Diagnostic> DiagnosticManager::filteredDiagnostics(const QList<Diagnostic> &diagnostics) const
{
    return diagnostics;
}

void DiagnosticManager::disableDiagnostics(TextEditor::TextDocument *document)
{

    Marks &marks = d->m_marks[document->filePath()];
    if (!marks.enabled)
        return;
    for (TextEditor::TextMark *mark : marks.marks)
        mark->setColor(Utils::Theme::Color::IconsDisabledColor);
    marks.enabled = false;
}

void DiagnosticManager::showDiagnostics(const FilePath &filePath, int version)
{
    if (TextDocument *doc = TextDocument::textDocumentForFilePath(filePath)) {
        QList<QTextEdit::ExtraSelection> extraSelections;
        const VersionedDiagnostics &versionedDiagnostics = d->m_diagnostics.value(filePath);
        if (versionedDiagnostics.version.value_or(version) == version
            && !versionedDiagnostics.diagnostics.isEmpty()) {
            Marks &marks = d->m_marks[filePath];
            const bool isProjectFile = d->m_client->fileBelongsToProject(filePath);
            for (const Diagnostic &diagnostic : versionedDiagnostics.diagnostics) {
                const QTextEdit::ExtraSelection selection
                    = createDiagnosticSelection(diagnostic, doc->document());
                if (!selection.cursor.isNull())
                    extraSelections << selection;
                if (TextEditor::TextMark *mark = createTextMark(doc, diagnostic, isProjectFile))
                    marks.marks.append(mark);
            }
            if (!marks.marks.isEmpty())
                emit textMarkCreated(filePath);
        }

        for (BaseTextEditor *editor : BaseTextEditor::textEditorsForDocument(doc))
            editor->editorWidget()->setExtraSelections(d->m_extraSelectionsId, extraSelections);
    }
}

Client *DiagnosticManager::client() const
{
    return d->m_client;
}

TextEditor::TextMark *DiagnosticManager::createTextMark(TextDocument *doc,
                                                        const Diagnostic &diagnostic,
                                                        bool /*isProjectFile*/) const
{
    static const QIcon icon = Icon::fromTheme("edit-copy");
    static const QString tooltip = Tr::tr("Copy to Clipboard");
    auto mark = new TextMark(doc, diagnostic, d->m_client);
    mark->setActionsProvider([text = diagnostic.message()] {
        QAction *action = new QAction();
        action->setIcon(icon);
        action->setToolTip(tooltip);
        QObject::connect(action, &QAction::triggered, [text] {
            setClipboardAndSelection(text);
        });
        return QList<QAction *>{action};
    });
    return mark;
}

QTextEdit::ExtraSelection DiagnosticManager::createDiagnosticSelection(
    const LanguageServerProtocol::Diagnostic &diagnostic, QTextDocument *textDocument) const
{
    QTextCursor cursor(textDocument);
    cursor.setPosition(diagnostic.range().start().toPositionInDocument(textDocument));
    cursor.setPosition(diagnostic.range().end().toPositionInDocument(textDocument),
                       QTextCursor::KeepAnchor);

    const FontSettings &fontSettings = TextEditorSettings::fontSettings();
    const DiagnosticSeverity severity = diagnostic.severity().value_or(DiagnosticSeverity::Warning);
    const TextStyle style = severity == DiagnosticSeverity::Error ? C_ERROR : C_WARNING;

    return QTextEdit::ExtraSelection{cursor, fontSettings.toTextCharFormat(style)};
}

void DiagnosticManager::setExtraSelectionsId(const Utils::Id &extraSelectionsId)
{
    // this function should be called before any diagnostics are handled
    QTC_CHECK(d->m_diagnostics.isEmpty());
    d->m_extraSelectionsId = extraSelectionsId;
}

void DiagnosticManager::forAllMarks(std::function<void (TextEditor::TextMark *)> func)
{
    for (const Marks &marks : std::as_const(d->m_marks)) {
        for (TextEditor::TextMark *mark : marks.marks)
            func(mark);
    }
}

void DiagnosticManager::clearDiagnostics()
{
    for (const Utils::FilePath &path : d->m_diagnostics.keys())
        hideDiagnostics(path);
    d->m_diagnostics.clear();
    QTC_ASSERT(d->m_marks.isEmpty(), d->m_marks.clear());
}

QList<Diagnostic> DiagnosticManager::diagnosticsAt(const FilePath &filePath,
                                                   const QTextCursor &cursor) const
{
    const int documentRevision = d->m_client->documentVersion(filePath);
    auto it = d->m_diagnostics.find(filePath);
    if (it == d->m_diagnostics.end())
        return {};
    if (documentRevision != it->version.value_or(documentRevision))
        return {};
    return Utils::filtered(it->diagnostics, [range = Range(cursor)](const Diagnostic &diagnostic) {
        return diagnostic.range().overlaps(range);
    });
}

bool DiagnosticManager::hasDiagnostic(const FilePath &filePath,
                                      const TextDocument *doc,
                                      const LanguageServerProtocol::Diagnostic &diag) const
{
    if (!doc)
        return false;
    const auto it = d->m_diagnostics.find(filePath);
    if (it == d->m_diagnostics.end())
        return {};
    const int revision = d->m_client->documentVersion(filePath);
    if (revision != it->version.value_or(revision))
        return false;
    return it->diagnostics.contains(diag);
}

bool DiagnosticManager::hasDiagnostics(const TextDocument *doc) const
{
    const FilePath docPath = doc->filePath();
    const auto it = d->m_diagnostics.find(docPath);
    if (it == d->m_diagnostics.end())
        return {};
    const int revision = d->m_client->documentVersion(docPath);
    if (revision != it->version.value_or(revision))
        return false;
    return !it->diagnostics.isEmpty();
}

} // namespace LanguageClient

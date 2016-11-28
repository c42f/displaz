// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_GLSL_HIGHLIGHTER_H_INCLUDED
#define DISPLAZ_GLSL_HIGHLIGHTER_H_INCLUDED

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QString>

/// Highlights GLSL code
class GLSLHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

    public:
        GLSLHighlighter(QTextDocument* parent = 0);

    protected:
        void highlightBlock(const QString& text) Q_DECL_OVERRIDE;

    private:
        enum class TextStyle
        {
            Plain,
            Italic,
        };

        /// States a section of text can have
        enum BlockState
        {
            // QSyntaxHighlighter defaults all text to -1
            Default = -1,
            MultilineComment,
        };

        /// Describes how text matching regexp should be formatted
        struct Rule
        {
            QRegExp regexp;
            QTextCharFormat format;
        };

        /// Adds a rule to m_rules
        void addRule(const QString& color, TextStyle style, const QString& pattern);

        QVector<Rule> m_rules;
        QTextCharFormat m_comment;
        QRegExp m_commentBegin; // Matches a /*
        QRegExp m_commentEnd; // Matches a */
};

#endif // DISPLAZ_GLSL_HIGHLIGHTER_H_INCLUDED

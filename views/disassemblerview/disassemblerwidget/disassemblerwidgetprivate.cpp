#include "disassemblerwidgetprivate.h"

DisassemblerWidgetPrivate::DisassemblerWidgetPrivate(QScrollArea *scrollarea, QScrollBar *vscrollbar, QWidget *parent): QWidget(parent), _scrollarea(scrollarea), _vscrollbar(vscrollbar), _printer(nullptr), _disassembler(nullptr), _listing(nullptr), _selectedblock(nullptr), _selectedindex(-1), _visiblelines(0), _clicked(false), _currentsegment(nullptr), _currentfunction(nullptr)
{
    this->_charwidth = this->_charheight = 0;

    QFont f("Monospace", qApp->font().pointSize());
    f.setStyleHint(QFont::TypeWriter); /* Use monospace fonts! */

    this->setFont(f);
    this->setWheelScrollLines(5);  /* By default: scroll 5 lines */
    this->setFocusPolicy(Qt::StrongFocus);
    this->setBackgroundRole(QPalette::Base);
    this->setAddressForeColor(Qt::darkBlue);
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    this->setSelectedLineColor(QColor(0xFF, 0xFF, 0xA0));

    connect(this->_vscrollbar, SIGNAL(valueChanged(int)), this, SLOT(updateLines(int)));
}

Block *DisassemblerWidgetPrivate::selectedBlock() const
{
    return this->_selectedblock;
}

void DisassemblerWidgetPrivate::setDisassembler(DisassemblerDefinition *disassembler)
{
    this->_disassembler = disassembler;
    this->_listing = disassembler->listing();
    this->_printer = new ListingPrinter(disassembler->addressType(), this);

    this->adjust();
    this->update();

    /* FIXME: Check Numero Istruzioni */
    if(!this->_listing->entryPoints().isEmpty()) // Select the first entry point, if any
    {
        qint64 idx = this->_listing->indexOf(this->_listing->entryPoints().first());
        this->setCurrentIndex(idx);
    }
}

void DisassemblerWidgetPrivate::setCurrentIndex(qint64 idx)
{
    if(idx == this->_selectedindex)
        return;

    if(idx < 0)
        idx = 0;
    else if(idx >= this->_listing->length())
        idx = this->_listing->length() - 1;

    const DisassemblerListing::BlockList& blocks = this->_listing->blocks();
    this->_selectedindex = idx;
    this->_selectedblock = blocks[idx];

    if(this->_disassembler && this->_listing)
        this->ensureVisible(idx);
}

void DisassemblerWidgetPrivate::setAddressForeColor(const QColor &c)
{
    this->_addressforecolor = c;
    this->update();
}

void DisassemblerWidgetPrivate::setSelectedLineColor(const QColor &c)
{
    this->_sellinecolor = c;
}

void DisassemblerWidgetPrivate::setWheelScrollLines(int c)
{
    this->_wheelscrolllines = c;
}

void DisassemblerWidgetPrivate::jumpTo(Block *block)
{
    qint64 idx = this->_listing->indexOf(block->startAddress(), block->blockType());

    if(idx != -1)
    {
        this->pushBack(this->_selectedindex); /* Save Previous Position */
        this->setCurrentIndex(idx);
    }
}

void DisassemblerWidgetPrivate::jumpTo(const DataValue& address)
{
    qint64 idx = this->_listing->indexOf(address);

    if(idx != -1)
    {
        this->pushBack(this->_selectedindex); /* Save Previous Position */
        this->setCurrentIndex(idx);
    }
}

void DisassemblerWidgetPrivate::clearNavigationHistory()
{
    this->_backstack.clear();
    this->_forwardstack.clear();

    emit backAvailable(false);
    emit forwardAvailable(false);
}

void DisassemblerWidgetPrivate::copy()
{
    if(!this->_disassembler)
        return;

    QString listing;
    QClipboard* clipboard = qApp->clipboard();

    if(this->_selectedblock->blockType() == Block::InstructionBlock)
        listing = this->_disassembler->emitInstruction(qobject_cast<Instruction*>(this->_selectedblock));
    else if(this->_selectedblock->blockType() == Block::FunctionBlock)
        listing = this->emitFunction(qobject_cast<Function*>(this->_selectedblock));
    else if(this->_selectedblock->blockType() == Block::SegmentBlock)
        listing = this->emitSegment(qobject_cast<Segment*>(this->_selectedblock));
    else if(this->_selectedblock->blockType() == Block::LabelBlock)
        listing = this->emitLabel(qobject_cast<Label*>(this->_selectedblock));

    clipboard->setText(QString("%1 %2").arg(this->_selectedblock->startAddress().toString(16), listing));
}

void DisassemblerWidgetPrivate::copyAddress()
{
    if(!this->_disassembler)
        return;

    QClipboard* clipboard = qApp->clipboard();
    clipboard->setText(this->_selectedblock->startAddress().toString(16));
}

void DisassemblerWidgetPrivate::back()
{
    if(this->_backstack.isEmpty())
        return;

    this->pushForward(this->_selectedindex);
    this->setCurrentIndex(this->popBack());
}

void DisassemblerWidgetPrivate::forward()
{
    if(this->_forwardstack.isEmpty())
        return;

    this->pushBack(this->_selectedindex);
    this->setCurrentIndex(this->popForward());
}

void DisassemblerWidgetPrivate::save(const QString &filename)
{
    if(!this->_disassembler || !this->_listing)
        return;

    QFile f(filename);
    f.open(QFile::WriteOnly | QFile::Truncate);

    for(qint64 i = 0; i < this->_listing->length(); i++)
        f.write(this->emitLine(i).append("\n").toUtf8());

    f.close();
}

qint64 DisassemblerWidgetPrivate::currentIndex() const
{
    return this->_selectedindex;
}

Block *DisassemblerWidgetPrivate::findBlock(qint64 idx)
{
    const DisassemblerListing::BlockList& blocks = this->_listing->blocks();

    if(idx < 0 || idx >= blocks.count())
    {
        throw PrefException("DisassemblerWidget::findBlock(): Index out of range");
        return nullptr;
    }

    Block* block = blocks[idx];

    if(block->blockType() == Block::SegmentBlock)
        this->_currentsegment = qobject_cast<Segment*>(block);
    else if(block->blockType() == Block::FunctionBlock)
        this->_currentfunction = qobject_cast<Function*>(block);

    if(!this->_currentsegment || !this->_currentsegment->contains(block->startAddress()))
        this->_currentsegment = this->_listing->findSegment(block);

    if(!this->_currentfunction || !this->_currentfunction->contains(block->startAddress()))
        this->_currentfunction = this->_listing->findFunction(block);

    return block;
}

QString DisassemblerWidgetPrivate::functionType(Function *f) const
{
    switch(f->type())
    {
        case FunctionType::EntryPointFunction:
            return "entrypoint";

        case FunctionType::ImportFunction:
            return "import";

        case FunctionType::ExportFunction:
            return "export";

        default:
            break;
    }

    return QString();
}

void DisassemblerWidgetPrivate::drawLineBackground(QPainter &painter, qint64 idx, int y)
{
    QRect linerect(0, y, this->width(), this->_charheight);

    if(idx == this->_selectedindex)
        painter.fillRect(linerect, this->_sellinecolor);
    else
        painter.fillRect(linerect, this->palette().color(QPalette::Base));
}

void DisassemblerWidgetPrivate::drawBookmark(QPainter &painter, QFontMetrics& fm, int y)
{
    painter.setPen(QPen(Qt::red));
    painter.drawText(0, y, this->_charwidth, fm.height(), Qt::TextExpandTabs | Qt::AlignLeft | Qt::AlignTop, "*");
}

void DisassemblerWidgetPrivate::drawLine(QPainter &painter, QFontMetrics &fm, qint64 idx, int y)
{
    Block* block = this->findBlock(idx);
    painter.setBackgroundMode(Qt::TransparentMode);
    this->drawLineBackground(painter, idx, y);

    QTextDocument document;

    if(block->isBookmarked())
        this->drawBookmark(painter, fm, y);

    int x = this->drawAddress(painter, fm, block, y);

    if(block->blockType() == Block::InstructionBlock)
    {
        this->drawInstruction(qobject_cast<Instruction*>(block), painter, fm, x + (this->_charwidth * 5), y);
        return;
    }

    switch(block->blockType())
    {
        case Block::SegmentBlock:
            document.setPlainText(this->emitSegment(qobject_cast<Segment*>(block)));
            break;

        case Block::FunctionBlock:
            document.setPlainText(this->emitFunction(qobject_cast<Function*>(block)));
            x += this->_charwidth * 2;
            break;

        case Block::LabelBlock:
            document.setPlainText(this->emitLabel(qobject_cast<Label*>(block)));
            x += this->_charwidth * 2;
            break;

        default:
            throw PrefException("DisassemblerWidget::drawLine(): Invalid Block Type");
            return;
    }

    DisassemblerHighlighter highlighter(&document, block);
    highlighter.rehighlight();  /* Apply Syntax Highlighting */

    painter.save();
        painter.translate(x, y - (this->_charheight - fm.ascent()));
        document.drawContents(&painter);
    painter.restore();
}

QString DisassemblerWidgetPrivate::emitSegment(Segment *segment)
{
    return QString("segment '%1' (Start Address: %2h, End Address: %3h)").arg(segment->name(), segment->startAddress().toString(16), segment->endAddress().toString(16));
}

QString DisassemblerWidgetPrivate::emitFunction(Function* func)
{
    SymbolTable* symboltable = this->_listing->symbolTable();
    return QString("%1 function %2()\t %3").arg(this->functionType(func), symboltable->name(func->startAddress()), this->displayReferences("Called by: ", func));
}

QString DisassemblerWidgetPrivate::emitLabel(Label *label)
{
    SymbolTable* symboltable = this->_listing->symbolTable();
    return QString("%1:\t\t%2").arg(symboltable->name(label->startAddress()), this->displayReferences("XREF: ", label));
}

void DisassemblerWidgetPrivate::drawInstruction(Instruction *instruction, QPainter &painter, const QFontMetrics &fm, int x, int y)
{
    this->_printer->reset();
    this->_disassembler->callOutput(this->_printer, instruction); /* Call Lua in order to compile instruction */
    this->_printer->draw(&painter, fm, x, y);
}

QString DisassemblerWidgetPrivate::displayReferences(const QString& prefix, Block* block)
{
    if(!block->hasSources())
        return QString();

    const QList<DataValue>& sources = block->sources();
    Segment* segment = nullptr;
    QString s = "# " + prefix;

    for(int i = 0; i < sources.count(); i++)
    {
        if(!segment || !segment->contains(sources[i]))
            segment = this->_listing->findSegment(sources[i]);

        QString segmentname = (segment ? segment->name() : "???");
        s.append(segmentname + ":" + sources[i].toString(16));

        if(i < (sources.count() - 1))
            s.append(" | ");
    }

    return s;
}


QString DisassemblerWidgetPrivate::emitLine(qint64 idx)
{
    QString blockstring;
    Block* block = this->findBlock(idx);

    if(block->blockType() == Block::InstructionBlock)
    {
        ListingPrinter lp(this->_disassembler->addressType());
        this->_disassembler->callOutput(&lp, qobject_cast<Instruction*>(block));
        blockstring = QString(" ").repeated(6) + lp.printString();
    }
    else if(block->blockType() == Block::SegmentBlock)
        blockstring = this->emitSegment(qobject_cast<Segment*>(block));
    else if(block->blockType() == Block::FunctionBlock)
        blockstring = QString(" ").repeated(2) + this->emitFunction(qobject_cast<Function*>(block));
    else if(block->blockType() == Block::LabelBlock)
        blockstring = QString(" ").repeated(4) + this->emitLabel(qobject_cast<Label*>(block));

    return QString("%1:%2 %3").arg(this->_currentsegment->name(), block->startAddress().toString(16), blockstring);
}

qint64 DisassemblerWidgetPrivate::visibleStart(QRect r) const
{
    if(r.isEmpty())
        r = this->rect();

    qint64 slidepos = this->_vscrollbar->isVisible() ? this->_vscrollbar->sliderPosition() : 0;
    return slidepos + (r.top() / this->_charheight);
}

qint64 DisassemblerWidgetPrivate::visibleEnd(QRect r) const
{
    if(r.isEmpty())
        r = this->rect();

    qint64 slidepos = this->_vscrollbar->isVisible() ? this->_vscrollbar->sliderPosition() : 0;
    return qMin(this->_listing->length() - 1, static_cast<qint64>(slidepos + (r.bottom() / this->_charheight) + 1)); /* end + 1 Removes the scroll bug */
}

int DisassemblerWidgetPrivate::drawAddress(QPainter &painter, QFontMetrics &fm, Block *block, int y)
{
    QString addrstring = QString("%1:%2").arg(this->_currentsegment->name(), block->startAddress().toString(16));
    int w = fm.width(addrstring);

    painter.setPen(this->_addressforecolor);
    painter.drawText(this->_charwidth, y, w, this->_charheight, Qt::AlignLeft | Qt::AlignTop, addrstring);

    return w + (this->_charwidth * 2);
}

void DisassemblerWidgetPrivate::adjust()
{
    QFontMetrics fm = this->fontMetrics();

    this->_charwidth = fm.width(" ");
    this->_charheight = fm.height();

    if(this->_disassembler && this->_listing)
    {
        this->_visiblelines = this->height() / this->_charheight;

        // Setup Vertical ScrollBar
        if(this->_listing->length() > this->_visiblelines)
        {
            this->_vscrollbar->setRange(0, (this->_listing->length() - this->_visiblelines) + 1);
            this->_vscrollbar->setSingleStep(1);
            this->_vscrollbar->setPageStep(this->_visiblelines);
            this->_vscrollbar->show();
        }
        else
            this->_vscrollbar->hide();
    }
    else
    {
        // No File Loaded: Hide Scroll Bars
        this->_vscrollbar->hide();
        this->setMinimumWidth(0);
    }
}

void DisassemblerWidgetPrivate::pushBack(qint64 idx)
{
    if(!this->_backstack.isEmpty() && (idx == this->_backstack.top())) /* Don't push duplicate indexes on stack */
        return;

    bool firesignal = this->_backstack.isEmpty();
    this->_backstack.push(idx);

    if(firesignal)
        emit backAvailable(true);
}

void DisassemblerWidgetPrivate::pushForward(qint64 idx)
{
    if(!this->_forwardstack.isEmpty() && (idx == this->_forwardstack.top())) /* Don't push duplicate indexes on stack */
        return;

    bool firesignal = this->_forwardstack.isEmpty();
    this->_forwardstack.push(idx);

    if(firesignal)
        emit forwardAvailable(true);
}

qint64 DisassemblerWidgetPrivate::popBack()
{
    qint64 idx = this->_backstack.pop();

    if(this->_backstack.isEmpty())
        emit backAvailable(false);

    return idx;
}

qint64 DisassemblerWidgetPrivate::popForward()
{
    qint64 idx = this->_forwardstack.pop();

    if(this->_forwardstack.isEmpty())
        emit forwardAvailable(false);

    return idx;
}

void DisassemblerWidgetPrivate::ensureVisible(qint64 idx)
{
    if(idx == -1)
        idx = 0;
    else if(idx >= this->_listing->length())
        idx = this->_listing->length() - 1;

    qint64 slidepos = static_cast<qint64>(this->_vscrollbar->sliderPosition());

    if(idx < slidepos)
        this->_vscrollbar->setValue(idx);
    else if(idx > (slidepos + this->_visiblelines))
        this->_vscrollbar->setValue(idx - 1);
    else
        this->update();
}

void DisassemblerWidgetPrivate::keyPressEvent(QKeyEvent *e)
{
    if(!this->_listing)
    {
        QWidget::keyPressEvent(e);
        return;
    }

    if(e->matches(QKeySequence::MoveToNextLine))
        this->setCurrentIndex(this->_selectedindex + 1);
    else if(e->matches(QKeySequence::MoveToPreviousLine))
        this->setCurrentIndex(this->_selectedindex - 1);
    else if(e->matches(QKeySequence::MoveToNextPage))
        this->setCurrentIndex(this->_selectedindex + ((this->height() / this->_charheight)) + 1);
    else if(e->matches(QKeySequence::MoveToPreviousPage))
        this->setCurrentIndex(this->_selectedindex - ((this->height() / this->_charheight)) + 1);
    else if(e->matches(QKeySequence::MoveToStartOfDocument))
        this->setCurrentIndex(0);
    else if(e->matches(QKeySequence::MoveToEndOfDocument))
        this->setCurrentIndex(this->_listing->length() - 1);
    else if(e->modifiers() == Qt::ControlModifier)
    {
        if(e->key() == Qt::Key_Left)
            this->back();
        else if(e->key() == Qt::Key_Right)
            this->forward();
        else if(e->key() == Qt::Key_C)
            this->copy();
        else if(e->key() == Qt::Key_G)
            emit jumpToRequested();
    }
    else if(e->modifiers() == Qt::NoModifier)
    {
        if(e->key() == Qt::Key_N)
            emit renameRequested(this->_selectedblock);
        else if(e->key() == Qt::Key_X)
            emit crossReferenceRequested(this->_selectedblock);
    }

    QWidget::keyPressEvent(e);
}

void DisassemblerWidgetPrivate::wheelEvent(QWheelEvent *e)
{
    if(this->_disassembler && this->_listing)
    {
        int numDegrees = e->delta() / 8;
        int numSteps = numDegrees / 15;

        if(e->orientation() == Qt::Vertical)
        {
            int pos = this->_vscrollbar->sliderPosition() - (numSteps * this->_wheelscrolllines);

            // Bounds Check
            if(pos < 0)
                pos = 0;
            else if(pos > this->_listing->length())
                pos = this->_listing->length() - 1;

            this->_vscrollbar->setValue(pos);
            e->accept();
        }
    }

    QWidget::wheelEvent(e);
}

void DisassemblerWidgetPrivate::paintEvent(QPaintEvent *pe)
{
    if(!this->_listing)
        return;

    QPainter painter(this);
    QRect r = pe->rect();
    QFontMetrics fm = this->fontMetrics();
    qint64 slidepos = this->_vscrollbar->isVisible() ? this->_vscrollbar->sliderPosition() : 0;
    qint64 start = this->visibleStart(r), end = this->visibleEnd(r);

    for(qint64 i = start; ((i <= end) && (i < this->_listing->length())); i++)
    {
        int y = (i - slidepos) * this->_charheight;
        this->drawLine(painter, fm, i, y);
    }
}

void DisassemblerWidgetPrivate::resizeEvent(QResizeEvent *)
{
    this->adjust(); /* Update ScrollBars */
}

void DisassemblerWidgetPrivate::mousePressEvent(QMouseEvent *e)
{
    if(!this->_clicked)
    {
        this->_clicked = true;
        QTimer::singleShot(qApp->doubleClickInterval(), this, SLOT(unlockClick()));

        if(this->_disassembler && this->_listing)
        {
            QPoint pos = e->pos();

            if(pos.y())
                this->setCurrentIndex(this->_vscrollbar->sliderPosition() + (pos.y() / this->_charheight));
        }
        else
            this->setCurrentIndex(-1);
    }

    QWidget::mousePressEvent(e);
}

void DisassemblerWidgetPrivate::mouseDoubleClickEvent(QMouseEvent *e)
{
    if(this->_disassembler && this->_listing)
    {
        QPoint pos = e->pos();

        if(pos.y())
        {
            const DisassemblerListing::BlockList& blocks = this->_listing->blocks();
            qint64 idx = this->_vscrollbar->sliderPosition() + (pos.y() / this->_charheight);
            Block* block = blocks[idx];

            if(block->blockType() == Block::InstructionBlock)
            {
                Instruction* instruction = qobject_cast<Instruction*>(block);

                if((instruction->isJump() || instruction->isCall()) && instruction->isDestinationValid() && this->_listing->isAddress(instruction->destination()))
                    this->jumpTo(instruction->destination());
            }
            else if((block->blockType() == Block::LabelBlock) && block->hasSources())
            {
                if(block->sources().count() == 1)
                    this->jumpTo(block->sources()[0]);
                else
                    emit crossReferenceRequested(block);
            }
        }
    }

    QWidget::mouseDoubleClickEvent(e);
}

void DisassemblerWidgetPrivate::unlockClick()
{
    this->_clicked = false;
}

void DisassemblerWidgetPrivate::updateLines(int)
{
    this->update();
}

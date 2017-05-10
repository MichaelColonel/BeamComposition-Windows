/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <TF1.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TGaxis.h>
#include <TLegend.h>

#include <QHeaderView>
#include <QDebug>
#include <QResizeEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "rootcanvasdialog.h"
#include "channelscountsfit.h"
#include "channelschargefit.h"

#include "signalvaluedelegate.h"
#include "chargevaluedelegate.h"

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

namespace {

#define HISTOGRAMS 7
struct Hist1Parameters histograms[HISTOGRAMS] = {
    { HIST_CHANNEL1, nullptr, nullptr, 200, 0., 400. },
    { HIST_CHANNEL2, nullptr, nullptr, 200, 0., 400. },
    { HIST_CHANNEL3, nullptr, nullptr, 200, 0., 400. },
    { HIST_CHANNEL4, nullptr, nullptr, 200, 0., 400. },
    { HIST_FIT, nullptr, nullptr, 200, 0., 400. },
    { HIST_SQRT_FIT, nullptr, nullptr, 200, 0., 400. },
    { HIST_Z, nullptr, nullptr, 200, 0.5, 7.5 },
};

} // namespace

double Hist1Parameters::energy_per_count = 2.0;

SettingsDialog::SettingsDialog(QWidget* parent)
    :
    QDialog(parent),
    ui(new Ui::SettingsDialog),
    hist1(nullptr),
    hist2(nullptr),
    settings(nullptr),
    selected_row(-1),
    lfit_ch(0),
    lfit_pos1(-1),
    lfit_pos2(-1)
{
    ui->setupUi(this);

    QHeaderView* hview = new QHeaderView( Qt::Vertical, ui->signalCountsTableWidget);
#if QT_VERSION >= 0x050000
    hview->setSectionsClickable(true);
#elif QT_VERSION >= 0x040000
    hview->setClickable(true);
#endif

    ui->signalCountsTableWidget->setVerticalHeader(hview);

    // Reference signal to Channel Counts Values
    SignalValueDelegate* reference_signal = new SignalValueDelegate(ui->signalCountsTableWidget);
    ui->signalCountsTableWidget->setItemDelegate(reference_signal);

    params = FitParameters::instance();
    ReferenceSignalMap& ref_signals = params->reference_signals();
    size_t elems = ref_signals.size();

    ui->signalCountsTableWidget->setRowCount(elems);
    int pos = 0;
//    for ( const auto& elem : ref_signals) {
    for ( ReferenceSignalMap::const_iterator it = ref_signals.begin(); it != ref_signals.end(); ++it) {

        int column = 0;

//        int value = elem.first;
        int value = it->first;
        QTableWidgetItem* item1 = new QTableWidgetItem(tr("%1").arg(value));
        ui->signalCountsTableWidget->setItem( pos, column++, item1);

//        for ( const SignalPair& sp : elem.second) {
        for ( SignalArray::const_iterator iter = it->second.begin(); iter != it->second.end(); ++iter) {
//            QString str = SignalValueDelegate::form_text(sp);
            QString str = SignalValueDelegate::form_text(*iter);
            QTableWidgetItem* item = new QTableWidgetItem(str);
            ui->signalCountsTableWidget->setItem( pos, column++, item);
        }
        pos++;
    }
/*
    for ( size_t i = 0; i < elems; ++i) {

        QTableWidgetItem* item1 = new QTableWidgetItem(tr("%1").arg(i));
        ui->signalCountsTableWidget->setItem( i, 0, item1);

        for ( int j = 1; j <= CHANNELS; ++j) {
            SignalPair p( 0.0, 0.0);
            QString str = SignalValueDelegate::form_text(p);
            QTableWidgetItem* item = new QTableWidgetItem(str);
            ui->signalCountsTableWidget->setItem( i, j, item);
        }
    }
*/
    ui->signalCountsTableWidget->addAction(ui->actionAddReferenceRowAbove);
    ui->signalCountsTableWidget->addAction(ui->actionAddReferenceRowBelow);
    QAction* act = new QAction(this);
    act->setSeparator(true);
    ui->signalCountsTableWidget->addAction(act);
    ui->signalCountsTableWidget->addAction(ui->actionDeleteReferenceRow);
    ui->signalCountsTableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

    // Ion charge to Channel Counts Values
    ChargeValueDelegate* charge_signal = new ChargeValueDelegate(ui->chargeCountsTableWidget);
    ui->chargeCountsTableWidget->setItemDelegate(charge_signal);

    ChargeSignalMap& ref_charge = params->charge_signals();

    const SignalPair& p = ref_charge[1];
    QString str = SignalValueDelegate::form_text(p);
    QTableWidgetItem* item = new QTableWidgetItem(str);
    ui->chargeCountsTableWidget->setItem( 0, 1, item);
/*
    for ( int i = 0; i < CARBON_Z; ++i) {
        const SignalPair& p = ref_charge[i + 1];
        QString str = SignalValueDelegate::form_text(p);
        QTableWidgetItem* item = new QTableWidgetItem(str);
        ui->chargeCountsTableWidget->setItem( i, 1, item);
    }
*/
    // Ion charge radius parameter
    const double* radius = params->charge_radius_parameter();
    for ( int i = 0; i < CARBON_Z; ++i) {
        QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(radius[i]));
        ui->chargeCountsTableWidget->setItem( i, 2, item);
    }

    // Ion charge beta parameter
    const double* beta = params->charge_beta_parameter();
    for ( int i = 0; i < CARBON_Z; ++i) {
        QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(beta[i]));
        ui->chargeCountsTableWidget->setItem( i, 3, item);
    }

    // fit charge
    const bool* fit_charge = CalibrationFitting::BeamCompositionFit::charge_in_fit();
    for ( int i = 0; i < CARBON_Z; ++i) {
//        QString text = (fit_charge[i]) ? tr("true") : tr("false");
        QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(int(fit_charge[i])));
        ui->chargeCountsTableWidget->setItem( i, 4, item);
    }

    connect( ui->tensionSpinBox, SIGNAL(valueChanged(double)), this, SLOT(tensionParameterChanged(double)));

    connect( ui->actionAddReferenceRowBelow, SIGNAL(triggered()), SLOT(signalCountsAddRowBelowClicked()));
    connect( ui->actionAddReferenceRowAbove, SIGNAL(triggered()), SLOT(signalCountsAddRowAboveClicked()));
    connect( ui->actionDeleteReferenceRow, SIGNAL(triggered()), SLOT(signalCountsDeleteRowClicked()));

    connect( ui->runDirectoryPushButton, SIGNAL(clicked()), this, SLOT(runDirectoryClicked()));
    connect( ui->writeRunFileCheckBox, SIGNAL(clicked(bool)), this, SLOT(writeRunChecked(bool)));
    connect( ui->restoreReferenceMatrixButton, SIGNAL(clicked()), this, SLOT(restoreReferenceMatrixClicked()));
    connect( ui->restoreReferenceChargeButton, SIGNAL(clicked()), this, SLOT(restoreReferenceChargeClicked()));
    connect( ui->showGraphMatrixButton, SIGNAL(clicked()), this, SLOT(showGraphClicked()));

    connect( this, SIGNAL(accepted()), this, SLOT(applyChanges()));
    connect( ui->channelComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelIndexChanged(int)));
    connect( ui->maxSpinBox, SIGNAL(valueChanged(double)), this, SLOT(maxChanged(double)));
    connect( ui->minSpinBox, SIGNAL(valueChanged(double)), this, SLOT(minChanged(double)));
    connect( ui->binSpinBox, SIGNAL(valueChanged(int)), this, SLOT(binsChanged(int)));
    connect( ui->signalCountsTableWidget, SIGNAL(cellClicked(int,int)), this, SLOT(signalCountsCellClicked( int, int)));
    connect( ui->signalCountsTableWidget, SIGNAL(cellChanged(int,int)), this, SLOT(signalCountsCellChanged( int, int)));
    connect( ui->chargeCountsTableWidget, SIGNAL(cellChanged(int,int)), this, SLOT(chargeCountsCellChanged( int, int)));

    connect( hview, SIGNAL(sectionClicked(int)), this, SLOT(signalCountsRowSelected(int)));

    ui->tensionSpinBox->setValue(params->tension());
    ui->energyCountSpinBox->setValue(Hist1Parameters::energy_per_count);
    const int* lfit = params->reference_channel_parameters(lfit_ch);
    lfit_pos1 = lfit[0];
    lfit_pos2 = lfit[1];
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void
SettingsDialog::runDirectoryClicked()
{
/*
    QFileDialog* dialog = new QFileDialog(this);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOption(QFileDialog::ShowDirsOnly);
    dialog->setDirectory(ui->RunDirectoryLineEdit->text());

    int res = dialog->exec();
    if (res == QDialog::Accepted) {
        QStringList files = dialog->selectedFiles();
        rundir = files[0];
        ui->RunDirectoryLineEdit->setText(rundir);
    }
    delete dialog;
*/
    QString start = ui->runDirectoryLineEdit->text();
    QString dir = QFileDialog::getExistingDirectory( this, tr("Select Directory"),
        start, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        rundir = dir;
        ui->runDirectoryLineEdit->setText(rundir);
    }
}

void
SettingsDialog::setSettingsParameters( QSettings* set,
    Hist1Parameters *h1params, Hist2Parameters *h2params)
{
    settings = set;
    hist1 = h1params;
    hist2 = h2params;

    bool write_run = settings->value( "write-run", true).toBool();
    ui->writeRunFileCheckBox->setChecked(write_run);

    rundir = settings->value( "run-directory", "/home").toString();
    ui->runDirectoryLineEdit->setText(rundir);

    for ( int i = 0; i < HISTOGRAMS; ++i) {
        histograms[i].bins = hist1[i].bins;
        histograms[i].min = hist1[i].min;
        histograms[i].max = hist1[i].max;
    }

    ui->channelComboBox->setCurrentIndex(0);

    ui->binSpinBox->setValue(histograms[0].bins);
    ui->maxSpinBox->setValue(histograms[0].max);
    ui->minSpinBox->setValue(histograms[0].min);

    settings->beginGroup("LinearFit");
    lfit_ch = settings->value( "reference-channel", 0).toInt();
    lfit_pos1 = settings->value( "reference-pos1", -1).toInt();
    lfit_pos2 = settings->value( "reference-pos2", -1).toInt();
    settings->endGroup();

    int timeout_peroid = settings->value( "update-timeout", 3).toInt();
    ui->timeoutPeriodSpinBox->setValue(timeout_peroid);

    int charge_min = settings->value( "min-fit-charge", 1).toInt();
    int charge_max = settings->value( "max-fit-charge", 6).toInt();
    ui->minChargeSpinBox->setValue(charge_min);
    ui->maxChargeSpinBox->setValue(charge_max);

//    double ecount = settings->value( "energy-per-count", 2.0).toDouble();
//    ui->energyCountSpinBox->setValue(ecount);
}

void
SettingsDialog::channelIndexChanged(int i)
{
    ui->binSpinBox->setValue(histograms[i].bins);
    ui->maxSpinBox->setValue(histograms[i].max);
    ui->minSpinBox->setValue(histograms[i].min);
}

void
SettingsDialog::minChanged(double value)
{
    int ch = ui->channelComboBox->currentIndex();
    histograms[ch].min = value;
}

void
SettingsDialog::maxChanged(double value)
{
    int ch = ui->channelComboBox->currentIndex();
    histograms[ch].max = value;
}

void
SettingsDialog::binsChanged(int value)
{
    int ch = ui->channelComboBox->currentIndex();
    histograms[ch].bins = value;
}

void
SettingsDialog::restoreReferenceMatrixClicked()
{
    int res = QMessageBox::warning( this, tr("Restore"),
        tr("Would you like to restore predefine matrix?"),
        QMessageBox::Yes, QMessageBox::No | QMessageBox::Default);
    if (res == QMessageBox::Yes) {
        params->restore_reference_signals();
        ui->signalCountsTableWidget->clear();
        ReferenceSignalMap& ref_signals = params->reference_signals();
        size_t elems = ref_signals.size();

        ui->signalCountsTableWidget->setRowCount(elems);
        int pos = 0;
//        for ( const auto& elem : ref_signals) {
        for ( ReferenceSignalMap::const_iterator it = ref_signals.begin(); it != ref_signals.end(); ++it) {

            int column = 0;

//            int value = elem.first;
            int value = it->first;
            QTableWidgetItem* item1 = new QTableWidgetItem(QString("%1").arg(value));
            ui->signalCountsTableWidget->setItem( pos, column++, item1);

//            for ( const SignalPair& sp : elem.second) {
//                QString str = SignalValueDelegate::form_text(sp);
            for ( SignalArray::const_iterator iter = it->second.begin(); iter != it->second.end(); ++iter) {
                QString str = SignalValueDelegate::form_text(*iter);
                QTableWidgetItem* item = new QTableWidgetItem(str);
                ui->signalCountsTableWidget->setItem( pos, column++, item);
            }
            pos++;
        }
        lfit_ch = 0;
        lfit_pos1 = -1;
        lfit_pos2 = -1;
    }
    else if (res == QMessageBox::No) {

    }
}

void
SettingsDialog::restoreReferenceChargeClicked()
{
    int res = QMessageBox::warning( this, tr("Restore"),
        tr("Would you like to restore predefine matrix?"),
        QMessageBox::Yes, QMessageBox::No | QMessageBox::Default);
    if (res == QMessageBox::Yes) {
        params->restore_charge_signals();
        ChargeSignalMap& ref_charge = params->charge_signals();

//        size_t elems = ref_signals.size();

//        ui->chargeCountsTableWidget->setRowCount(elems);
/*
        for ( int i = 0; i < CARBON_Z; ++i) {
            const SignalPair& p = ref_charge[i + 1];
            QString str = SignalValueDelegate::form_text(p);
            QTableWidgetItem* item = new QTableWidgetItem(str);
            ui->chargeCountsTableWidget->setItem( i, 1, item);
        }
*/
        const SignalPair& p = ref_charge[1];
        QString str = SignalValueDelegate::form_text(p);
        QTableWidgetItem* item = ui->chargeCountsTableWidget->item( 0, 1);
        item->setText(str);

        // Ion charge radius parameter
        const double* radius = params->charge_radius_parameter();
        for ( int i = 0; i < CARBON_Z; ++i) {
            QTableWidgetItem* item = ui->chargeCountsTableWidget->item( i, 2);
            item->setText(QString("%1").arg(radius[i]));
        }

        // Ion charge beta parameter
        const double* beta = params->charge_beta_parameter();
        for ( int i = 0; i < CARBON_Z; ++i) {
            QTableWidgetItem* item = ui->chargeCountsTableWidget->item( i, 3);
            item->setText(QString("%1").arg(beta[i]));
        }
    }
    else if (res == QMessageBox::No) {

    }
}

void
SettingsDialog::applyChanges()
{
    params->tension() = ui->tensionSpinBox->value();

    settings->setValue( "update-timeout", ui->timeoutPeriodSpinBox->value());
    settings->setValue( "min-fit-charge", ui->minChargeSpinBox->value());
    settings->setValue( "max-fit-charge", ui->maxChargeSpinBox->value());

    settings->setValue( "run-directory", ui->runDirectoryLineEdit->text());
    settings->setValue( "energy-per-count", ui->energyCountSpinBox->value());
    Hist1Parameters::energy_per_count = ui->energyCountSpinBox->value();

/*
    // recalculate parameters if parameters are set
    if (lfit_ch > 0 && lfit_pos1 != -1 && lfit_pos1 != -1) {
        recalculate_linear_fit();
        settings->beginGroup("LinearFit");
        settings->setValue( "reference-channel", lfit_ch);
        settings->setValue( "reference-pos1", lfit_pos1);
        settings->setValue( "reference-pos2", lfit_pos2);
        settings->setValue( "parameter-a", ChannelsCountsFit::lfit[0]);
        settings->setValue( "parameter-b", ChannelsCountsFit::lfit[1]);
        settings->endGroup();
    }
*/
    for ( int i = 0; i < HISTOGRAMS; ++i) {
        Hist1Parameters& p = histograms[i];
        hist1[i].setBins( p.bins, p.min, p.max);
    }

    // set parameters for fitted ranks
    Hist1Parameters& fitted = histograms[4];
    for ( int i = HISTOGRAMS; i < HISTOGRAMS + CHANNELS; ++i) {
        hist1[i].setBins( fitted.bins, fitted.min, fitted.max);
    }

    // parameters for channel correlation
    Hist1Parameters& c1 = hist1[0];
    Hist1Parameters& c2 = hist1[1];
    Hist1Parameters& c3 = hist1[2];
    Hist1Parameters& c4 = hist1[3];

    Hist2Parameters& c12 = hist2[0];
    c12.setBinsX(c1);
    c12.setBinsY(c2);

    Hist2Parameters& c23 = hist2[1];
    c23.setBinsX(c2);
    c23.setBinsY(c3);

    Hist2Parameters& c34 = hist2[2];
    c34.setBinsX(c3);
    c34.setBinsY(c4);

    Hist2Parameters& c14 = hist2[3];
    c14.setBinsX(c1);
    c14.setBinsY(c4);

    Hist2Parameters& c13 = hist2[4];
    c13.setBinsX(c1);
    c13.setBinsY(c3);

    Hist2Parameters& c24 = hist2[5];
    c24.setBinsX(c2);
    c24.setBinsY(c4);

    // parameters for charge correlation
    Hist1Parameters& z = histograms[6];
    for ( int i = 6; i < 12; ++i) {
        Hist2Parameters& zz = hist2[i];
        zz.setBinsX(z);
        zz.setBinsY(z);
    }

    int rows = ui->signalCountsTableWidget->rowCount();
    int columns = ui->signalCountsTableWidget->columnCount();

    ReferenceSignalMap& ref_signals = params->reference_signals();
    ref_signals.clear();

    for ( int i = 0; i < rows; ++i) {
        QTableWidgetItem* key_item = ui->signalCountsTableWidget->item( i, 0);
        QString key_text = key_item->text();
        double key_value = QVariant(key_text).toDouble();
        SignalArray arr;
        for ( int j = 1; j < columns; ++j) {
            QTableWidgetItem* signal_item = ui->signalCountsTableWidget->item( i, j);
            QString signal_text = signal_item->text();
            SignalPair sp = SignalValueDelegate::parse_text(signal_text);
            arr[j - 1] = sp;
        }
        ref_signals[key_value] = arr;
    }

    // recalculate fitting
    if (lfit_ch > 0 && lfit_pos1 != -1 && lfit_pos2 != -1) {
        params->recalculate( lfit_ch, lfit_pos1, lfit_pos2);
    }
    else // default (channel-4, pos 0, 1)
        params->recalculate( 4, 0, 3);

    const double* lfit = params->fit_parameters();
    qDebug() << QString(tr("Fit line equation: %1*x+%2").arg(lfit[0]).arg(lfit[1]));
}

void
SettingsDialog::writeRunChecked(bool state)
{
    settings->setValue( "write-run", state);
}

void
SettingsDialog::signalCountsCellChanged( int row, int column)
{
    QTableWidgetItem* item = ui->signalCountsTableWidget->item( row, column);
    qDebug() << item->text();
}

void
SettingsDialog::chargeCountsCellChanged( int row, int column)
{
    if (column == 1) {
        QTableWidgetItem* item = ui->chargeCountsTableWidget->item( row, column);
        SignalPair charge_signal_pair = SignalValueDelegate::parse_text(item->text());

        ChargeSignalMap& ref_charge = params->charge_signals();
        SignalPair& charge_pair = ref_charge[row + 1];
        charge_pair = charge_signal_pair;
    }
    else if (column == 2) { // charge radius
        QTableWidgetItem* item = ui->chargeCountsTableWidget->item( row, column);
        QVariant variant(item->text());
        double value = variant.toDouble();

        double* radius = params->charge_radius_parameter();
        radius[row] = value;
        qDebug() << "Charge radius: " << value;
    }
    else if (column == 3) { // charge beta
        QTableWidgetItem* item = ui->chargeCountsTableWidget->item( row, column);
        QVariant variant(item->text());
        double value = variant.toDouble();

        double* beta = params->charge_beta_parameter();
        beta[row] = value;
        qDebug() << "Charge beta: " << value;
    }
    else if (column == 4) { // use charge in fit
        QTableWidgetItem* item = ui->chargeCountsTableWidget->item( row, column);
        QVariant variant(item->text());
        bool value = variant.toBool();

        bool* fit_charge = CalibrationFitting::BeamCompositionFit::charge_in_fit();
        fit_charge[row] = value;
        qDebug() << "Use charge in fit: " << value;
    }
}

void
SettingsDialog::signalCountsAddRowAboveClicked()
{
    if (selected_row != -1) {
        addSignalCountsRow(selected_row);
    }
}

void
SettingsDialog::signalCountsAddRowBelowClicked()
{
    if (selected_row != -1) {
        addSignalCountsRow(++selected_row);
    }
}

void
SettingsDialog::addSignalCountsRow(int row)
{
    ui->signalCountsTableWidget->insertRow(row);

    int columns = ui->signalCountsTableWidget->columnCount();
    for ( int i = 0; i < columns; ++i) {
        if (i) {
            QTableWidgetItem* item = new QTableWidgetItem();
            SignalPair p( 0.0, 0.0);
            QString str = SignalValueDelegate::form_text(p);
            item->setText(str);
            ui->signalCountsTableWidget->setItem( selected_row, i, item);
        }
        else {
            QTableWidgetItem* item_above = ui->signalCountsTableWidget->item( selected_row - 1, i);
            QTableWidgetItem* item_below = ui->signalCountsTableWidget->item( selected_row + 1, i);

            QString key_above = item_above->text();
            double value_above = QVariant(key_above).toDouble();

            QString key_below = item_below->text();
            double value_below = QVariant(key_below).toDouble();
            QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg((value_above + value_below) / 2.));
            ui->signalCountsTableWidget->setItem( selected_row, i, item);
        }
    }
}

void
SettingsDialog::signalCountsDeleteRowClicked()
{
    qDebug() << "delete";
    if (selected_row != -1) {
        ui->signalCountsTableWidget->removeRow(selected_row);
    }
}

void
SettingsDialog::signalCountsRowSelected(int r)
{
    selected_row = r;
    qDebug() << r;
    int rows = ui->signalCountsTableWidget->rowCount();
    ui->actionDeleteReferenceRow->setEnabled(rows);

    ui->actionAddReferenceRowAbove->setEnabled(selected_row);
    bool end_of_table = (selected_row == (rows - 1));
    ui->actionAddReferenceRowBelow->setEnabled(!end_of_table);
}

void
SettingsDialog::tensionParameterChanged(double t)
{
    params->tension() = t;
}

void
SettingsDialog::showGraphClicked()
{
/*
    QList<QTableWidgetItem*> list = ui->signalCountsTableWidget->selectedItems();
    int pos1 = 0;
    int pos2 = 0;
    int column = 0;
    if (list.size() == 2) {
        QList<QTableWidgetItem*>::iterator it = list.begin();

        QTableWidgetItem* item1 = *it;
        ++it;
        QTableWidgetItem* item2 = *it;

        if (item1->column() > 0 && item2->column() > 0 &&
                item1->column() == item2->column()) {
            pos1 = item1->row();
            pos2 = item2->row();
            column = item1->column();
        }
    }
*/
    // Reference signal to channels count relation
    TMultiGraph* mg = new TMultiGraph( "mg", "");

    RootCanvasDialog* dialog = new RootCanvasDialog(this);

    double* x[CHANNELS] = {};
    double* xerr[CHANNELS] = {};
    double* y[CHANNELS] = {};
    double* yerr[CHANNELS] = {};
    TGraphErrors* err[CHANNELS] = {};
    Color_t colors[CHANNELS] = { kRed, kBlue, kViolet, kGreen };
/*
    ReferenceSignalMap& rsmap = ChannelsCountsFit::reference_signals;
    size_t sz = ChannelsCountsFit::reference_signals.size();
    for ( int i = 0; i < CHANNELS; ++i) {
        x[i] = new double[sz];
        xerr[i] = new double[sz];
        y[i] = new double[sz];
        yerr[i] = new double[sz];

        double* x_ = x[i];
        double* xerr_ = xerr[i];
        double* y_ = y[i];
        double* yerr_ = yerr[i];

        size_t j = 0;
        for ( auto it = rsmap.begin(); it != rsmap.end(); ++it) {
            x_[j] = (*it).first;
            xerr_[j] = 0.0;
            const SignalArray& array = (*it).second;
            y_[j] = array[i].first;
            yerr_[j] = array[i].second;
            ++j;
        }
        err[i] = new TGraphErrors( sz, x_, y_, xerr_, yerr_);
        err[i]->SetMarkerColor(colors[i]);
        err[i]->SetMarkerStyle(21);
        mg->Add( err[i], "lp");
    }
*/

    int rows = ui->signalCountsTableWidget->rowCount();

//    const SignalArray& back = params->background();
    for ( int i = 0; i < CHANNELS; ++i) {
        x[i] = new double[rows];
        xerr[i] = new double[rows];
        y[i] = new double[rows];
        yerr[i] = new double[rows];

        double* x_ = x[i];
        double* xerr_ = xerr[i];
        double* y_ = y[i];
        double* yerr_ = yerr[i];

        // table background for channels
        QTableWidgetItem* back_item = ui->signalCountsTableWidget->item( 0, i + 1);
        QString back_text = back_item->text();
        SignalPair ref_back = SignalValueDelegate::parse_text(back_text);

        for ( int j = 0; j < rows; ++j) {
            QTableWidgetItem* key_item = ui->signalCountsTableWidget->item( j, 0);
            QString key_text = key_item->text();
            double key_value = QVariant(key_text).toDouble();

            QTableWidgetItem* signal_item = ui->signalCountsTableWidget->item( j, i + 1);
            QString signal_text = signal_item->text();
            SignalPair sp = SignalValueDelegate::parse_text(signal_text);
            x_[j] = key_value;
            xerr_[j] = 0.0;
//            y_[j] = sp.first;// - ChannelsCountsFit::background[i].first;
//            y_[j] = sp.first - back[i].first;
            y_[j] = sp.first - ref_back.first;
//            y_[j] += back[0].first;
            yerr_[j] = 3. * sp.second;
        }
        err[i] = new TGraphErrors( rows, x_, y_, xerr_, yerr_);
        err[i]->SetLineColor(colors[i]);
        err[i]->SetLineWidth(2);
        err[i]->SetMarkerColor(colors[i]);
        err[i]->SetMarkerStyle(21);
        mg->Add( err[i], "lp");
    }

    // final line transformation
    int ch = lfit_ch - 1;
    int& pos1 = lfit_pos1;
    int& pos2 = lfit_pos2;
    double lfit[2] = {};
    lfit[0] = (y[ch][pos1] - y[ch][pos2]) / (x[ch][pos1] - x[ch][pos2]);
    lfit[1] = y[ch][pos1] - lfit[0] * x[ch][pos1];

    // set linear fit parameters
    params->set_linear_fit_parameters( lfit[0], lfit[1]);

    TF1 *fline = new TF1( "line", "[0]*x+[1]", x[0][0], x[0][rows - 1]);
//    TF1 *fpoly = new TF1( "c1poly", "pol3", x[0][0], x[0][rows - 1]);

    fline->SetLineColor(kBlack);
    fline->SetParameters(lfit);

    qDebug() << QString(tr("Fit line equation: %1*x+%2").arg(lfit[0]).arg(lfit[1]));

    TLegend* leg = new TLegend( 0.15, 0.5, 0.35, 0.85, "Channels");
    leg->AddEntry( err[0], "Channel-1", "l");
    leg->AddEntry( err[1], "Channel-2", "l");
    leg->AddEntry( err[2], "Channel-3", "l");
    leg->AddEntry( err[3], "Channel-4", "l");
    leg->AddEntry( fline, "Fit line", "l");

    // draw multigraph in the canvas pad
    TPad* pad = dialog->getPad();

    pad->cd();
    mg->Draw("ap");

    mg->GetHistogram()->SetMaximum(4095.0);   // along
    mg->GetHistogram()->SetMinimum(0.0);  //   Y

    mg->GetXaxis()->SetTitleOffset(1.2);
    mg->GetXaxis()->SetTitleSize(0.03);
    mg->GetXaxis()->SetLabelSize(0.03);
    mg->GetYaxis()->SetTitleOffset(1.3);
    mg->GetXaxis()->SetTitle("Reference signal (mV)");
    mg->GetYaxis()->SetTitle("Amplitude (ADC count)");

    leg->Draw();
    fline->Draw("lsame");

//    err[0]->Fit( "c1poly", "R");
//    fpoly->Draw("lsame");
/*
    double emin = 0.0;
    double emax = ui->energyCountSpinBox->value() * 4095.0 * 1.E-03;

    double xmin = x[0][0];
    double xmax = (4095.0 - lfit[1]) / lfit[0];

    TGaxis *axis = new TGaxis( xmin, 4095.0, xmax, 4095.0, emin, emax, 50510, "-LC");
    axis->SetTitleSize(0.03);
    axis->SetLabelSize(0.035);
    axis->SetTitleOffset(1.3);
    axis->SetName("energy_axis");
    axis->SetTitle("Energy (MeV)");
    axis->SetLabelColor(kMagenta - 3);
    axis->SetTitleColor(kMagenta - 3);
    axis->Draw();
*/
    dialog->setModal(true);
    dialog->exec();

    for ( int i = 0; i < CHANNELS; ++i) {
        delete [] x[i];
        delete [] xerr[i];
        delete [] y[i];
        delete [] yerr[i];
        delete err[i];
    }

    delete fline;
//    delete fpoly;
    delete mg;
    delete leg;
//    delete axis;

    delete dialog;

}

void
SettingsDialog::signalCountsCellClicked( int, int)
{
    // enable or disable linear fit calculation
    QList<QTableWidgetItem*> list = ui->signalCountsTableWidget->selectedItems();
    int pos1 = -1;
    int pos2 = -1;
    int column = 0;
    if (list.size() == 2) {
        QList<QTableWidgetItem*>::iterator it = list.begin();

        QTableWidgetItem* item1 = *it;
        ++it;
        QTableWidgetItem* item2 = *it;

        if (item1->column() > 0 &&
                item2->column() > 0 &&
                item1->column() == item2->column()) {
            pos1 = item1->row();
            pos2 = item2->row();
            column = item1->column();
        }
    }
    if (pos1 != -1 && pos2 != -1 && column) {
        lfit_pos1 = pos1;
        lfit_pos2 = pos2;
        lfit_ch = column;
        ui->showGraphMatrixButton->setEnabled(true);
    }
    else {
        lfit_pos1 = -1;
        lfit_pos2 = -1;
        lfit_ch = 0;
        ui->showGraphMatrixButton->setEnabled(false);
    }
}

void
SettingsDialog::recalculate_linear_fit()
{
/*
    QList<QTableWidgetItem*> list = ui->signalCountsTableWidget->selectedItems();
    int pos1 = 0;
    int pos2 = 0;
    int column = 0;
    if (list.size() == 2) {
        QList<QTableWidgetItem*>::iterator it = list.begin();

        QTableWidgetItem* item1 = *it;
        ++it;
        QTableWidgetItem* item2 = *it;

        if (item1->column() > 0 && item2->column() > 0 &&
                item1->column() == item2->column()) {
            pos1 = item1->row();
            pos2 = item2->row();
            column = item1->column();
        }
    }
*/

    double* x[CHANNELS] = {};
    double* y[CHANNELS] = {};

    int rows = ui->signalCountsTableWidget->rowCount();

    for ( int i = 0; i < CHANNELS; ++i) {
        x[i] = new double[rows];
        y[i] = new double[rows];

        double* x_ = x[i];
        double* y_ = y[i];
        for ( int j = 0; j < rows; ++j) {
            QTableWidgetItem* key_item = ui->signalCountsTableWidget->item( j, 0);
            QString key_text = key_item->text();
            double key_value = QVariant(key_text).toDouble();

            QTableWidgetItem* signal_item = ui->signalCountsTableWidget->item( j, i + 1);
            QString signal_text = signal_item->text();
            SignalPair sp = SignalValueDelegate::parse_text(signal_text);
            x_[j] = key_value;
            y_[j] = sp.first;// - ChannelsCountsFit::background[i].first;
        }
    }

    // final line transformation
    int ch = lfit_ch - 1;
    int& pos1 = lfit_pos1;
    int& pos2 = lfit_pos2;
    double lfit[2] = {};
    lfit[0] = (y[ch][pos1] - y[ch][pos2]) / (x[ch][pos1] - x[ch][pos2]);
    lfit[1] = y[ch][pos1] - lfit[0] * x[ch][pos1];

    // set linear fit parameters
    params->set_linear_fit_parameters( lfit[0], lfit[1]);

    for ( int i = 0; i < CHANNELS; ++i) {
        delete [] x[i];
        delete [] y[i];
    }
}

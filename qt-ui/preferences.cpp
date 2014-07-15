#include "preferences.h"
#include "mainwindow.h"
#include <QSettings>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QShortcut>
#include <QNetworkProxy>

PreferencesDialog *PreferencesDialog::instance()
{
	static PreferencesDialog *dialog = new PreferencesDialog(MainWindow::instance());
	dialog->setAttribute(Qt::WA_QuitOnClose, false);
	LanguageModel::instance();
	return dialog;
}

PreferencesDialog::PreferencesDialog(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f)
{
	ui.setupUi(this);
	ui.proxyType->clear();
	ui.proxyType->addItem(tr("No proxy"), QNetworkProxy::NoProxy);
	ui.proxyType->addItem(tr("System proxy"), QNetworkProxy::DefaultProxy);
	ui.proxyType->addItem(tr("HTTP proxy"), QNetworkProxy::HttpProxy);
	ui.proxyType->addItem(tr("SOCKS proxy"), QNetworkProxy::Socks5Proxy);
	ui.proxyType->setCurrentIndex(-1);
	connect(ui.proxyType, SIGNAL(currentIndexChanged(int)), this, SLOT(proxyType_changed(int)));
	connect(ui.buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonClicked(QAbstractButton *)));
	connect(ui.gflow, SIGNAL(valueChanged(int)), this, SLOT(gflowChanged(int)));
	connect(ui.gfhigh, SIGNAL(valueChanged(int)), this, SLOT(gfhighChanged(int)));
	QShortcut *close = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), this);
	connect(close, SIGNAL(activated()), this, SLOT(close()));
	QShortcut *quit = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this);
	connect(quit, SIGNAL(activated()), parent, SLOT(close()));
	loadSettings();
	setUiFromPrefs();
	rememberPrefs();
}

#define DANGER_GF (gf > 100) ? "* { color: red; }" : ""
void PreferencesDialog::gflowChanged(int gf)
{
	ui.gflow->setStyleSheet(DANGER_GF);
}
void PreferencesDialog::gfhighChanged(int gf)
{
	ui.gfhigh->setStyleSheet(DANGER_GF);
}
#undef DANGER_GF

void PreferencesDialog::showEvent(QShowEvent *event)
{
	setUiFromPrefs();
	rememberPrefs();
	QDialog::showEvent(event);
}

void PreferencesDialog::setUiFromPrefs()
{
	// graphs
	ui.pheThreshold->setValue(prefs.pp_graphs.phe_threshold);
	ui.po2Threshold->setValue(prefs.pp_graphs.po2_threshold);
	ui.pn2Threshold->setValue(prefs.pp_graphs.pn2_threshold);
	ui.maxpo2->setValue(prefs.modpO2);
	ui.red_ceiling->setChecked(prefs.redceiling);
	ui.units_group->setEnabled(ui.personalize->isChecked());

	ui.gflow->setValue(prefs.gflow);
	ui.gfhigh->setValue(prefs.gfhigh);
	ui.gf_low_at_maxdepth->setChecked(prefs.gf_low_at_maxdepth);

	// units
	if (prefs.unit_system == METRIC)
		ui.metric->setChecked(true);
	else if (prefs.unit_system == IMPERIAL)
		ui.imperial->setChecked(true);
	else
		ui.personalize->setChecked(true);

	ui.celsius->setChecked(prefs.units.temperature == units::CELSIUS);
	ui.fahrenheit->setChecked(prefs.units.temperature == units::FAHRENHEIT);
	ui.meter->setChecked(prefs.units.length == units::METERS);
	ui.feet->setChecked(prefs.units.length == units::FEET);
	ui.bar->setChecked(prefs.units.pressure == units::BAR);
	ui.psi->setChecked(prefs.units.pressure == units::PSI);
	ui.liter->setChecked(prefs.units.volume == units::LITER);
	ui.cuft->setChecked(prefs.units.volume == units::CUFT);
	ui.kg->setChecked(prefs.units.weight == units::KG);
	ui.lbs->setChecked(prefs.units.weight == units::LBS);

	ui.font->setCurrentFont(QString(prefs.divelist_font));
	ui.fontsize->setValue(prefs.font_size);
	ui.defaultfilename->setText(prefs.default_filename);
	ui.default_cylinder->clear();
	for (int i = 0; tank_info[i].name != NULL; i++) {
		ui.default_cylinder->addItem(tank_info[i].name);
		if (prefs.default_cylinder && strcmp(tank_info[i].name, prefs.default_cylinder) == 0)
			ui.default_cylinder->setCurrentIndex(i);
	}
	ui.displayinvalid->setChecked(prefs.display_invalid_dives);
	ui.display_unused_tanks->setChecked(prefs.display_unused_tanks);
	ui.show_average_depth->setChecked(prefs.show_average_depth);
	ui.vertical_speed_minutes->setChecked(prefs.units.vertical_speed_time == units::MINUTES);
	ui.vertical_speed_seconds->setChecked(prefs.units.vertical_speed_time == units::SECONDS);
	ui.velocitySlider->setValue(prefs.animation);

	QSortFilterProxyModel *filterModel = new QSortFilterProxyModel();
	filterModel->setSourceModel(LanguageModel::instance());
	filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	ui.languageView->setModel(filterModel);
	filterModel->sort(0);
	connect(ui.languageFilter, SIGNAL(textChanged(QString)), filterModel, SLOT(setFilterFixedString(QString)));

	QSettings s;

	ui.save_uid_local->setChecked(s.value("save_uid_local").toBool());
	ui.default_uid->setText(s.value("subsurface_webservice_uid").toString().toUpper());

	s.beginGroup("Language");
	ui.languageSystemDefault->setChecked(s.value("UseSystemLanguage", true).toBool());
	QAbstractItemModel *m = ui.languageView->model();
	QModelIndexList languages = m->match(m->index(0, 0), Qt::UserRole, s.value("UiLanguage").toString());
	if (languages.count())
		ui.languageView->setCurrentIndex(languages.first());

	s.endGroup();

	ui.proxyHost->setText(prefs.proxy_host);
	ui.proxyPort->setValue(prefs.proxy_port);
	ui.proxyAuthRequired->setChecked(prefs.proxy_auth);
	ui.proxyUsername->setText(prefs.proxy_user);
	ui.proxyPassword->setText(prefs.proxy_pass);
	ui.proxyType->setCurrentIndex(ui.proxyType->findData(prefs.proxy_type));
}

void PreferencesDialog::restorePrefs()
{
	prefs = oldPrefs;
	setUiFromPrefs();
}

void PreferencesDialog::rememberPrefs()
{
	oldPrefs = prefs;
}

#define SB(V, B) s.setValue(V, (int)(B->isChecked() ? 1 : 0))


#define GET_UNIT(name, field, f, t)                                 \
	v = s.value(QString(name));                                 \
	if (v.isValid())                                            \
		prefs.units.field = (v.toInt() == (t)) ? (t) : (f); \
	else                                                        \
		prefs.units.field = default_prefs.units.field

#define GET_BOOL(name, field)                           \
	v = s.value(QString(name));                     \
	if (v.isValid())                                \
		prefs.field = v.toBool();               \
	else                                            \
		prefs.field = default_prefs.field

#define GET_DOUBLE(name, field)             \
	v = s.value(QString(name));         \
	if (v.isValid())                    \
		prefs.field = v.toDouble(); \
	else                                \
		prefs.field = default_prefs.field

#define GET_INT(name, field)             \
	v = s.value(QString(name));      \
	if (v.isValid())                 \
		prefs.field = v.toInt(); \
	else                             \
		prefs.field = default_prefs.field

#define GET_INT_DEF(name, field, defval)                                             \
	v = s.value(QString(name));                                      \
	if (v.isValid())                                                 \
		prefs.field = v.toInt(); \
	else                                                             \
		prefs.field = defval

#define GET_TXT(name, field)                                             \
	v = s.value(QString(name));                                      \
	if (v.isValid())                                                 \
		prefs.field = strdup(v.toString().toUtf8().constData()); \
	else                                                             \
		prefs.field = default_prefs.field

void PreferencesDialog::syncSettings()
{
	QSettings s;

	s.setValue("subsurface_webservice_uid", ui.default_uid->text().toUpper());
	set_save_userid_local(ui.save_uid_local->checkState());

	// Graph
	s.beginGroup("TecDetails");
	s.setValue("phethreshold", ui.pheThreshold->value());
	s.setValue("po2threshold", ui.po2Threshold->value());
	s.setValue("pn2threshold", ui.pn2Threshold->value());
	s.setValue("modpO2", ui.maxpo2->value());
	SB("redceiling", ui.red_ceiling);
	s.setValue("gflow", ui.gflow->value());
	s.setValue("gfhigh", ui.gfhigh->value());
	SB("gf_low_at_maxdepth", ui.gf_low_at_maxdepth);
	SB("display_unused_tanks", ui.display_unused_tanks);
	SB("show_average_depth", ui.show_average_depth);
	s.endGroup();

	// Units
	s.beginGroup("Units");
	QString unitSystem = ui.metric->isChecked() ? "metric" : (ui.imperial->isChecked() ? "imperial" : "personal");
	s.setValue("unit_system", unitSystem);
	s.setValue("temperature", ui.fahrenheit->isChecked() ? units::FAHRENHEIT : units::CELSIUS);
	s.setValue("length", ui.feet->isChecked() ? units::FEET : units::METERS);
	s.setValue("pressure", ui.psi->isChecked() ? units::PSI : units::BAR);
	s.setValue("volume", ui.cuft->isChecked() ? units::CUFT : units::LITER);
	s.setValue("weight", ui.lbs->isChecked() ? units::LBS : units::KG);
	s.setValue("vertical_speed_time", ui.vertical_speed_minutes->isChecked() ? units::MINUTES : units::SECONDS);
	s.endGroup();

	// Defaults
	s.beginGroup("GeneralSettings");
	s.setValue("default_filename", ui.defaultfilename->text());
	s.setValue("default_cylinder", ui.default_cylinder->currentText());
	s.endGroup();

	s.beginGroup("Display");
	s.setValue("divelist_font", ui.font->currentFont());
	s.setValue("font_size", ui.fontsize->value());
	s.setValue("displayinvalid", ui.displayinvalid->isChecked());
	s.endGroup();
	s.sync();

	// Locale
	QLocale loc;
	s.beginGroup("Language");
	bool useSystemLang = s.value("UseSystemLanguage", true).toBool();
	if (useSystemLang != ui.languageSystemDefault->isChecked() ||
	    (!useSystemLang && s.value("UiLanguage").toString() != ui.languageView->currentIndex().data(Qt::UserRole))) {
		QMessageBox::warning(MainWindow::instance(), tr("Restart required"),
				     tr("To correctly load a new language you must restart Subsurface."));
	}
	s.setValue("UseSystemLanguage", ui.languageSystemDefault->isChecked());
	s.setValue("UiLanguage", ui.languageView->currentIndex().data(Qt::UserRole));
	s.endGroup();

	// Animation
	s.beginGroup("Animations");
	s.setValue("animation_speed", ui.velocitySlider->value());
	s.endGroup();

	s.beginGroup("Network");
	s.setValue("proxy_type", ui.proxyType->itemData(ui.proxyType->currentIndex()).toInt());
	s.setValue("proxy_host", ui.proxyHost->text());
	s.setValue("proxy_port", ui.proxyPort->value());
	SB("proxy_auth", ui.proxyAuthRequired);
	s.setValue("proxy_user", ui.proxyUsername->text());
	s.setValue("proxy_pass", ui.proxyPassword->text());
	s.endGroup();

	loadSettings();
	emit settingsChanged();
}

void PreferencesDialog::loadSettings()
{
	// This code was on the mainwindow, it should belong nowhere, but since we dind't
	// correctly fixed this code yet ( too much stuff on the code calling preferences )
	// force this here.

	QSettings s;
	QVariant v;

	ui.save_uid_local->setChecked(s.value("save_uid_local").toBool());
	ui.default_uid->setText(s.value("subsurface_webservice_uid").toString().toUpper());

	s.beginGroup("Units");
	if (s.value("unit_system").toString() == "metric") {
		prefs.unit_system = METRIC;
		prefs.units = SI_units;
	} else if (s.value("unit_system").toString() == "imperial") {
		prefs.unit_system = IMPERIAL;
		prefs.units = IMPERIAL_units;
	} else {
		prefs.unit_system = PERSONALIZE;
		GET_UNIT("length", length, units::FEET, units::METERS);
		GET_UNIT("pressure", pressure, units::PSI, units::BAR);
		GET_UNIT("volume", volume, units::CUFT, units::LITER);
		GET_UNIT("temperature", temperature, units::FAHRENHEIT, units::CELSIUS);
		GET_UNIT("weight", weight, units::LBS, units::KG);
	}
	GET_UNIT("vertical_speed_time", vertical_speed_time, units::MINUTES, units::SECONDS);
	s.endGroup();
	s.beginGroup("TecDetails");
	GET_BOOL("po2graph", pp_graphs.po2);
	GET_BOOL("pn2graph", pp_graphs.pn2);
	GET_BOOL("phegraph", pp_graphs.phe);
	GET_DOUBLE("po2threshold", pp_graphs.po2_threshold);
	GET_DOUBLE("pn2threshold", pp_graphs.pn2_threshold);
	GET_DOUBLE("phethreshold", pp_graphs.phe_threshold);
	GET_BOOL("mod", mod);
	GET_DOUBLE("modpO2", modpO2);
	GET_BOOL("ead", ead);
	GET_BOOL("redceiling", redceiling);
	GET_BOOL("dcceiling", dcceiling);
	GET_BOOL("calcceiling", calcceiling);
	GET_BOOL("calcceiling3m", calcceiling3m);
	GET_BOOL("calcndltts", calcndltts);
	GET_BOOL("calcalltissues", calcalltissues);
	GET_BOOL("hrgraph", hrgraph);
	GET_INT("gflow", gflow);
	GET_INT("gfhigh", gfhigh);
	GET_BOOL("gf_low_at_maxdepth", gf_low_at_maxdepth);
	GET_BOOL("zoomed_plot", zoomed_plot);
	set_gf(prefs.gflow, prefs.gfhigh, prefs.gf_low_at_maxdepth);
	GET_BOOL("show_sac", show_sac);
	GET_BOOL("display_unused_tanks", display_unused_tanks);
	GET_BOOL("show_average_depth", show_average_depth);
	s.endGroup();

	s.beginGroup("GeneralSettings");
	GET_TXT("default_filename", default_filename);
	GET_TXT("default_cylinder", default_cylinder);
	s.endGroup();

	s.beginGroup("Display");
	QFont defaultFont = s.value("divelist_font", qApp->font()).value<QFont>();
	defaultFont.setPointSizeF(s.value("font_size", qApp->font().pointSizeF()).toFloat());
	qApp->setFont(defaultFont);

	GET_TXT("divelist_font", divelist_font);
	GET_INT("font_size", font_size);
	if (prefs.font_size < 0)
		prefs.font_size = defaultFont.pointSizeF();
	GET_INT("displayinvalid", display_invalid_dives);
	s.endGroup();

	s.beginGroup("Animations");
	GET_INT("animation_speed", animation);
	s.endGroup();

	s.beginGroup("Network");
	GET_INT_DEF("proxy_type", proxy_type, QNetworkProxy::DefaultProxy);
	GET_TXT("proxy_host", proxy_host);
	GET_INT("proxy_port", proxy_port);
	GET_BOOL("proxy_auth", proxy_auth);
	GET_TXT("proxy_user", proxy_user);
	GET_TXT("proxy_pass", proxy_pass);
	s.endGroup();
}

void PreferencesDialog::buttonClicked(QAbstractButton *button)
{
	switch (ui.buttonBox->standardButton(button)) {
	case QDialogButtonBox::Discard:
		restorePrefs();
		syncSettings();
		close();
		break;
	case QDialogButtonBox::Apply:
		syncSettings();
		break;
	case QDialogButtonBox::FirstButton:
		syncSettings();
		close();
		break;
	default:
		break; // ignore warnings.
	}
}
#undef SB

void PreferencesDialog::on_chooseFile_clicked()
{
	QFileInfo fi(system_default_filename());
	QString choosenFileName = QFileDialog::getOpenFileName(this, tr("Open default log file"), fi.absolutePath(), tr("Subsurface XML files (*.ssrf *.xml *.XML)"));

	if (!choosenFileName.isEmpty())
		ui.defaultfilename->setText(choosenFileName);
}

void PreferencesDialog::on_resetSettings_clicked()
{
	QSettings s;

	QMessageBox response(this);
	response.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	response.setDefaultButton(QMessageBox::Cancel);
	response.setWindowTitle(tr("Warning"));
	response.setText(tr("If you click OK, all settings of Subsurface will be reset to their default values. This will be applied immediately."));
	response.setWindowModality(Qt::WindowModal);

	int result = response.exec();
	if (result == QMessageBox::Ok) {
		prefs = default_prefs;
		setUiFromPrefs();
		Q_FOREACH (QString key, s.allKeys()) {
			s.remove(key);
		}
		syncSettings();
		close();
	}
}

void PreferencesDialog::emitSettingsChanged()
{
	emit settingsChanged();
}

void PreferencesDialog::proxyType_changed(int idx)
{
	if (idx == -1) {
		return;
	}

	int proxyType = ui.proxyType->itemData(idx).toInt();
	bool hpEnabled = (proxyType == QNetworkProxy::Socks5Proxy || proxyType == QNetworkProxy::HttpProxy);
	ui.proxyHost->setEnabled(hpEnabled);
	ui.proxyPort->setEnabled(hpEnabled);
	ui.proxyAuthRequired->setEnabled(hpEnabled);
	ui.proxyUsername->setEnabled(hpEnabled & ui.proxyAuthRequired->isChecked());
	ui.proxyPassword->setEnabled(hpEnabled & ui.proxyAuthRequired->isChecked());
	ui.proxyAuthRequired->setChecked(ui.proxyAuthRequired->isChecked());
}

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QGridLayout>
#include <QMenu>
#include <QMenuBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QFont>

#include <voice_synth.h>
#include <QAudioOutput>
#include <QAudioDeviceInfo>
#include <QBuffer>
#include <QByteArray>
#include <QSettings>

#include <QDebug>

class MainWindow : public QMainWindow
{
	Q_OBJECT
	QListWidget *textList = new QListWidget(this);
	QLineEdit *textEdit = new QLineEdit(this);
	QPushButton *saveBtn = new QPushButton("Save", this);
	QPushButton *speakBtn = new QPushButton("Speak", this);
	QComboBox *outputBox = new QComboBox(this);
	QComboBox *voiceBox = new QComboBox(this);

	QAudioFormat audioFormat;
	QAudioOutput *audioOut = nullptr;
	QBuffer *audioBuffer = new QBuffer(this);

	typedef struct SpeakBtnControl_t{
		QPushButton *speakBtn = nullptr;
		SpeakBtnControl_t(QPushButton *btn){
			this->speakBtn = btn;
			this->speakBtn->setDisabled(true);
			qDebug() << "Disabled: true";
		}
		~SpeakBtnControl_t(){
			if(this->speakBtn != nullptr){
				this->speakBtn->setDisabled(false);
				qDebug() << "Disabled: false";
			}
		}
	}SpeakBtnControl;

	void createMenu(){
		QMenu *menu = new QMenu("Edit", this);
		this->menuBar()->addMenu(menu);

		menu->addAction("Add new text", [this](){
			bool ok = false;
			QString text = QInputDialog::getText(this, "Text", "Enter a text", QLineEdit::Normal, "", &ok);
			if(!ok || text.isEmpty()){
				return;
			}
			this->textList->addItem(text);
			if(this->textList->count() > 0){
				this->textList->setCurrentRow(this->textList->count() - 1);
			}
		});

		menu->addAction("Delete text", [this](){
			int currentRow = this->textList->currentRow();
			if(currentRow < 0){
				return;
			}
			QListWidgetItem *item = this->textList->takeItem(currentRow);
			if(item != nullptr){
				delete item;
				item = nullptr;
			}
			if(this->textList->count() > 0){
				this->textList->setCurrentRow(this->textList->count() -1);
			}else{
				this->textList->clearSelection();
				this->textEdit->clear();
			}

		});
	}

	void createTriggers(){
		this->connect(this->textList, &QListWidget::currentRowChanged, [this](int currentRow){
			if(currentRow >= 0){
				this->textEdit->setText(this->textList->currentItem()->text());
			}
		});

		this->connect(this->saveBtn, &QPushButton::clicked, [this](int checked){
			Q_UNUSED(checked);
			QString text = this->textEdit->text();
			if(text.isEmpty()){
				return;
			}
			int currentRow = this->textList->currentRow();
			if(currentRow >= 0){
				this->textList->currentItem()->setText(text);
			}else{
				this->textList->addItem(text);
			}
		});

		this->connect(this->speakBtn, &QPushButton::clicked, [this](int checked){
			Q_UNUSED(checked);
			SpeakBtnControl speakBtnControl(this->speakBtn);
			if(this->textEdit->text().isEmpty()){
				return;
			}
			QString text = this->textEdit->text();
			std::wstring wText = text.toStdWString();

			QString selectedVoiceTokenId = this->voiceBox->currentData().toString();
			std::wstring voiceName = selectedVoiceTokenId.toStdWString();
			const wchar_t *voiceNameCStr = selectedVoiceTokenId.isEmpty() ? nullptr : voiceName.c_str();

			VoiceData voice = {};
			bool ok = synthesize_text(wText.c_str(), voiceNameCStr, &voice);

			if(!ok || !voice.data || voice.size <= 0){
				QMessageBox::critical(this, "Failed to create speech", "Failed to synthesize voice");
				return;
			}
			qDebug() << "Audio size:" << voice.size << "bytes";
			qDebug() << "Duration approx:" << (voice.size / (16000 * 2)) << "seconds";

			if(this->audioBuffer != nullptr){
				this->audioBuffer->close();
				delete this->audioBuffer;
				this->audioBuffer = nullptr;
			}

			QByteArray audioData(reinterpret_cast<const char*>(voice.data), voice.size);
			if(this->audioBuffer == nullptr){
				this->audioBuffer = new QBuffer(this);
			}
			this->audioBuffer->setData(audioData);
			this->audioBuffer->open(QIODevice::ReadOnly);
			this->audioBuffer->seek(0);

			this->audioOut->start(this->audioBuffer);

			free_voice_data(&voice);
		});

	}

	void setupAudio(){
		this->audioFormat.setSampleRate(16000);
		this->audioFormat.setChannelCount(1);
		this->audioFormat.setSampleSize(16);
		this->audioFormat.setCodec("audio/pcm");
		this->audioFormat.setByteOrder(QAudioFormat::LittleEndian);
		this->audioFormat.setSampleType(QAudioFormat::SignedInt);

		this->audioOut = new QAudioOutput(audioFormat, this);

	}

	void setupAudioOutput(){
		QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
		for(const QAudioDeviceInfo &device: devices){
			this->outputBox->addItem(device.deviceName(), QVariant::fromValue(device));
		}

		this->connect(this->outputBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index){
			Q_UNUSED(index);
			QAudioDeviceInfo selectedDevice = this->outputBox->currentData().value<QAudioDeviceInfo>();

			if(this->audioOut != nullptr){
				delete this->audioOut;
				this->audioOut = new QAudioOutput(selectedDevice, this->audioFormat, this);
				qDebug() << "Audio output selected:" << selectedDevice.deviceName();
			}
		});
	}

	void setupInstalledVoices(){
		static const QString normalVoiceRegPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices\\Tokens\\";
		static const QString compableVoiceRegPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\";

		QSettings normalVoice(normalVoiceRegPath, QSettings::NativeFormat);
		for(const QString &token: normalVoice.childGroups()){
			QSettings tokenKey(normalVoiceRegPath + token + "\\Attributes", QSettings::NativeFormat);
			QString display = tokenKey.value("Name").toString();
			this->voiceBox->addItem(display + "(Normal)", token);
		}

		QSettings compableVoice(compableVoiceRegPath, QSettings::NativeFormat);
		for(const QString &token: compableVoice.childGroups()){
			QSettings tokenKey(compableVoiceRegPath + token + "\\Attributes", QSettings::NativeFormat);
			QString display = tokenKey.value("Name").toString();
			this->voiceBox->addItem(display + "(Compable)", token);
		}
	}

public:
	explicit MainWindow(QWidget *parent = nullptr): QMainWindow(parent){
		QFont font = QApplication::font();
		font.setPixelSize(22);
		font.setFamily("Microsoft YaHei");
		QApplication::setFont(font);

		this->resize(640, 480);

		this->textList->setSelectionBehavior(QAbstractItemView::SelectRows);
		this->textList->setSelectionMode(QAbstractItemView::SingleSelection);
		this->createMenu();

		QWidget *wid = new QWidget(this);
		QGridLayout *lay = new QGridLayout;
		wid->setLayout(lay);

		lay->addWidget(this->outputBox, 0, 0, 1, 3);
		lay->addWidget(this->textList, 1, 0, 1, 3);
		lay->addWidget(this->textEdit, 2, 0, 1, 1);
		lay->addWidget(this->saveBtn, 2, 1, 1, 1);
		lay->addWidget(this->speakBtn, 2, 2, 1, 1);
		lay->addWidget(this->voiceBox, 3, 0, 1, 3);

		this->setupAudio();
		this->setupAudioOutput();
		this->setupInstalledVoices();

		this->createTriggers();

		this->setCentralWidget(wid);
	}
	~MainWindow(){ }
};
#endif // MAINWINDOW_H

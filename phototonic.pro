TEMPLATE = app
TARGET = phototonic
INCLUDEPATH += .
INCLUDEPATH += /usr/local/include
INCLUDEPATH += C:/Users/xilin/development/exiv2-0.25/include
LIBS += -L/usr/local/lib -LC:/Users/xilin/development/exiv2-0.25/msvc2005/bin/x64/DebugDLL -llibexiv2 
QT += widgets
QMAKE_CXXFLAGS += $$(CXXFLAGS)
QMAKE_CFLAGS += $$(CFLAGS)
QMAKE_LFLAGS += $$(LDFLAGS)

DEFINES += EXV_UNICODE_PATH

# Input
HEADERS += dialogs.h mainwindow.h thumbview.h imageview.h croprubberband.h global.h infoview.h \
			fstree.h bookmarks.h dircompleter.h tags.h mdcache.h
SOURCES += dialogs.cpp main.cpp mainwindow.cpp thumbview.cpp imageview.cpp croprubberband.cpp \
			global.cpp infoview.cpp fstree.cpp bookmarks.cpp dircompleter.cpp tags.cpp \
			mdcache.cpp
RESOURCES += phototonic.qrc

target.path = /usr/bin/

icon.files = images/phototonic.png
icon.path = /usr/share/icons/hicolor/48x48/apps

icon16.files = images/icon16/phototonic.png
icon16.path = /usr/share/icons/hicolor/16x16/apps

iconPixmaps.files = images/icon16/phototonic.png
iconPixmaps.path = /usr/share/pixmaps

desktop.files = phototonic.desktop
desktop.path = /usr/share/applications

INSTALLS += target icon icon16 iconPixmaps desktop

TRANSLATIONS = 	translations/phototonic_en.ts \
		translations/phototonic_pl.ts \
		translations/phototonic_de.ts \
		translations/phototonic_ru.ts \
		translations/phototonic_cs.ts \
		translations/phototonic_fr.ts \
		translations/phototonic_bs.ts \
		translations/phototonic_hr.ts \
		translations/phototonic_sr.ts


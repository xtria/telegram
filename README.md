### sigram

A different telegram client from Sialan.Labs.

Sigram project are released under the terms of the GPLv3 license.

### How to Compile
#### Install dependencies

Install gcc, g++, openssl, Qt5Core, Qt5DBus, Qt5Gui, Qt5Multimedia, Qt5MultimediaQuick_p, Qt5Network, Qt5PrintSupport, Qt5Qml, Qt5Quick, Qt5Sql, Qt5Svg, Qt5Widgets, and libcurl4-openssl-dev.
on Ubuntu:

    sudo apt-get install g++ gcc qtbase5-dev libqt5sql5-sqlite libqt5multimediaquick-p5 libqt5multimedia5-plugins libqt5multimedia5 libqt5qml5 libqt5qml-graphicaleffects libqt5qml-quickcontrols qtdeclarative5-dev libqt5quick5 libcurl4-openssl-dev

on Fedora (tested on Fedora 20):

    yum install qt5-qtbase qt5-qtbase-devel qt5-qtdeclarative qt5-qtquick1 qt5-qtquick1-devel kde-plasma-applicationname kde-plasma-nm qt5-qtdeclarative-devel qt5-qtdeclarative-static qt5-qtgraphicaleffects qt5-qtquickcontrols openssl-devel libappindicator-devel

Also you should install AppIndicator and Gtk2 if you want to enable UnitySystemTray plugin.

    sudo apt-get install libappindicator-dev libgtk2.0-dev

For other distributions search for the corresponding packages.

#### Get source code from git repository

If you want get source from git repository you should install git on your system:

    sudo apt-get install git

After git installed, get code with this command:

    git clone https://github.com/sialan-labs/sigram.git

#### Start building

Switch to source directory

    cd sigram

##### Ubuntu

    mkdir build && cd build
    qmake -r .. PREFIX=/usr
    make
    make install

If you want to build UnitySystemTray plugin run this command:

    qmake -r .. EXTENSIONS+=unity PREFIX=/usr
    make
    make install

You can use command below after building to clean build directory on the each step.

    make clean

##### Fedora

    mkdir build && cd build
    /bin/qmake-qt5 -o Makefile ../SialanTelegram.pro
    make

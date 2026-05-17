#include <QApplication>
#include "mainwindow.h"
// #include "config.h"

#ifdef _WIN32
#include <windows.h>
#endif

// #include "pckeyer.h"

int main(int argc, char *argv[])
{
    #ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    #endif

    QApplication app(argc, argv);
    
    app.setStyleSheet(
        "QMainWindow { background-color: #333; }"
        "QMessageBox { background-color: #333; color: white; }"
    );
    
    //  qDebug() << "CWTalk PCKeyer Test";
    // qDebug() << "Waiting for event loop to start...";
    
    // PCKeyer keyer;
    // keyer.setPortName("COM8");      // 修改为你的串口
    // keyer.setKeyingLine("RTS");     // DTR 或 RTS
    // keyer.setActiveLow(false);      // 高电平有效
    // keyer.setWpm(20);
    
    // // 连接调试输出
    // QObject::connect(&keyer, &QObject::destroyed, [&]() {
    //     qDebug() << "Keyer destroyed";
    // });
    
    // // 1 秒后打开并发送
    // QTimer::singleShot(1000, &app, [&]() {
    //     qDebug() << "Opening serial port...";
        
    //     if (!keyer.open()) {
    //         qDebug() << "ERROR: Failed to open COM4";
    //         // qDebug() << "Available ports:";
    //         // for (const auto &info : QSerialPortInfo::availablePorts()) {
    //         //     qDebug() << "  " << info.portName() << info.description();
    //         // }
    //         app.quit();
    //         return;
    //     }
        
    //     qDebug() << "OK: Port opened, sending 'CQ'...";
    //     keyer.sendString("CQ CQ CQ ");
    // });
    
    // 5 秒后检查状态
    // QTimer::singleShot(6000, &app, [&]() {
    //     qDebug() << "Status: isSending =" << keyer.isSending();
    //     qDebug() << "Buffer:" << keyer.buffer();
    //     qDebug() << "Current index:" << keyer.currentIndex();
        
    //     keyer.close();
    //     app.quit();
    // });

    MainWindow window;
    window.show();
    
    return app.exec();
}
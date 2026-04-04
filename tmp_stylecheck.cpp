#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget popup;
    popup.setObjectName("notificationPopup");

    auto *card = new QFrame(&popup);
    card->setObjectName("notificationCard");
    auto *cardLayout = new QHBoxLayout(card);

    auto *icon = new QLabel(card);
    icon->setObjectName("iconLabel");
    cardLayout->addWidget(icon);

    auto *textLayout = new QVBoxLayout();
    cardLayout->addLayout(textLayout);

    auto *summary = new QLabel(card);
    summary->setObjectName("summaryLabel");
    textLayout->addWidget(summary);

    auto *body = new QLabel(card);
    body->setObjectName("bodyLabel");
    textLayout->addWidget(body);

    popup.setStyleSheet(
        "QWidget#notificationPopup {\n"
        "    background: transparent;\n"
        "}\n"
        "QFrame#notificationCard {\n"
        "    background-color: #D91e1e2e;\n"
        "    border: 3px solid #cba6f7;\n"
        "    border-radius: 0;\n"
        "}\n"
        "QLabel#summaryLabel {\n"
        "    color: #cdd6f4;\n"
        "    font-size: 16px;\n"
        "    font-weight: 700;\n"
        "}\n"
        "QLabel#bodyLabel {\n"
        "    color: #cdd6f4;\n"
        "    font-size: 13px;\n"
        "    line-height: 1.35em;\n"
        "}\n"
        "QLabel#iconLabel {\n"
        "    min-width: 0px;\n"
        "    max-width: 16777215px;\n"
        "}\n");

    return 0;
}

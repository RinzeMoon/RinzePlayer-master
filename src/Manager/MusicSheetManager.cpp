//
// Created by lsy on 2025/10/25.
//

#include "../../Header/Manager/MusicSheetManager.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>

#include <../Global/Global.h>
using AudioUtil::SongMeta;
using AudioUtil::MusicSheet;

QList<MusicSheet> MusicSheetManager::loadAllSheetsFromLocalByJSON() {
    QList<MusicSheet> sheetList; // 存储解析后的所有歌单

    // 1. 检查文件夹是否存在，不存在则返回空列表
    QDir sheetDir(m_localStoragePath);
    if (!sheetDir.exists()) {
        qDebug() << "歌单文件夹不存在：" << m_localStoragePath;
        return sheetList;
    }

    // 2. 获取文件夹中所有.json文件
    QStringList jsonFilters;
    jsonFilters << "*.json";
    QFileInfoList jsonFiles = sheetDir.entryInfoList(jsonFilters, QDir::Files | QDir::Readable);

    if (jsonFiles.isEmpty()) {
        qDebug() << "文件夹中无JSON歌单文件：" << m_localStoragePath;
        return sheetList;
    }

    // 3. 逐个解析文件为MusicSheet，添加到列表
    for (const QFileInfo& fileInfo : jsonFiles) {
        QFile file(fileInfo.filePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "无法打开文件（跳过）：" << fileInfo.fileName() << "，错误：" << file.errorString();
            continue;
        }

        // 读取JSON内容
        QByteArray jsonData = file.readAll();
        file.close();

        // 解析JSON文档
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isNull() || !doc.isObject()) {
            qDebug() << "JSON格式错误（跳过）：" << fileInfo.fileName();
            continue;
        }

        // 调用你的fromJson方法，转换为MusicSheet
        MusicSheet sheet = AudioUtil::fromJson(doc.object());
        if (sheet.id.isEmpty()) {
            qDebug() << "歌单ID为空（跳过）：" << fileInfo.fileName();
            continue;
        }

        // 添加到列表
        sheetList.append(sheet);
        qDebug() << "解析歌单成功：" << sheet.title << "（ID：" << sheet.id << "）";
    }

    qDebug() << "加载完成，共解析" << sheetList.size() << "个有效歌单";
    return sheetList;
}

void MusicSheetManager::loadSheetsFromItemState() {
    QString itemStatePath = QFileInfo(m_localStoragePath).path() + "/itemMeta/item_state.json";
    QFile file(itemStatePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 item_state.json 文件:" << itemStatePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "item_state.json 格式错误";
        return;
    }

    QJsonArray itemsArray = doc.array();
    qDebug() << "从 item_state.json 发现" << itemsArray.size() << "个歌单项";

    for (const QJsonValue& val : itemsArray) {
        QJsonObject itemObj = val.toObject();
        QString sheetId = itemObj["sheetId"].toString();

        if (!sheetId.isEmpty() && !m_sheets.contains(sheetId)) {
            qDebug() << "加载歌单数据:" << sheetId;
            loadSheetByIdFromLocal(sheetId);
        }
    }
}

void MusicSheetManager::onRemoveSongFromSheet(const QString& sheetId, const QString& songId) {
    qDebug() << "MusicSheetManager: 从歌单删除歌曲" << sheetId << "歌曲ID:" << songId;

    if (!hasSheet(sheetId)) {
        qWarning() << "歌单不存在:" << sheetId;
        return;
    }

    MusicSheet& sheet = m_sheets[sheetId];

    // 查找歌曲并删除
    bool found = false;
    QString songName;
    for (int i = 0; i < sheet.songs.size(); ++i) {
        if (sheet.songs[i].id == songId) {
            songName = sheet.songs[i].name;
            sheet.songs.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qDebug() << "未在歌单中找到歌曲，歌曲ID:" << songId;
        return;
    }

    // 更新歌曲计数
    sheet.songCount = sheet.songs.size();

    // 保存到本地文件
    if (saveSheetsToLocalById(sheetId)) {
        qDebug() << "歌曲删除成功并已保存到JSON，歌曲:" << songName;
        emit sheetDataChanged(sheetId);
        emit sheetSaved(sheetId);
    } else {
        qWarning() << "歌曲删除成功，但保存到JSON失败";
    }
}
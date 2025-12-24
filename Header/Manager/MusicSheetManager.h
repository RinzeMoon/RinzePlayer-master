//
// Created by lsy on 2025/10/25.
//

#ifndef RINZEPLAYER_MUSICSHEETMANAGER_H
#define RINZEPLAYER_MUSICSHEETMANAGER_H
#include <QObject>
#include <QJsonDocument>

#include <Global.h>

using AudioUtil::MusicSheet;
using AudioUtil::SongMeta;

class MusicSheetManager : public QObject {
    Q_OBJECT
public:
    // 单例模式，全局唯一实例
    static MusicSheetManager* getInstance() {
        static MusicSheetManager instance;
        return &instance;
    }

    // 加载歌单数据（从文件/数据库/网络）
    void loadSheetsFromDB() {

    }

    void loadSheetsFromNet()
    {

    }

    bool updateSheetToNet()
    {
        return false;
    }

    // 根据歌单ID获取完整歌单数据
    MusicSheet& getSheetById(const QString& sheetId)
    {
        // 只有当内存中不存在该歌单时，才从本地加载（避免覆盖已更新的内存数据）
        if (!m_sheets.contains(sheetId)) {
            loadSheetByIdFromLocal(sheetId); // 内存中没有，才加载本地文件
        }
        qDebug() << "[MusicSheet]: get结果 :" << m_sheets[sheetId].title << "&" << m_sheets[sheetId].id;
        return m_sheets[sheetId]; // 返回内存中的数据（可能是刚加载的，或已更新的）
    }

    // 获取所有歌单的基础信息（用于左侧列表展示：ID + 标题）
    QList<QPair<QString, QString>> getAllSheetInfos() {
        // 确保从 item_state.json 加载所有相关歌单
        loadSheetsFromItemState();

        QList<QPair<QString, QString>> infos;
        qDebug() << "=== getAllSheetInfos - 当前m_sheets数量:" << m_sheets.size() << "===";

        for (const auto& sheet : m_sheets) {
            qDebug() << "歌单 - ID:" << sheet.id << "标题:" << sheet.title;
            infos.append({sheet.id, sheet.title});
        }

        qDebug() << "返回歌单信息数量:" << infos.size();
        return infos;
    }

    // 从本地文件加载所有歌单
    QList<MusicSheet> loadAllSheetsFromLocalByJSON();

    bool loadSheetByIdFromLocal(const QString& sheetId) {
        // 1. 构建目标文件路径（与保存路径一致）
        QString sheetDir = QFileInfo(m_localStoragePath).path() + "/SheetsMeta";
        QString targetFilePath = sheetDir + "/" + sheetId + ".json";

        // 2. 检查文件是否存在
        if (!QFile::exists(targetFilePath)) {
            qWarning() << "歌单文件不存在：" << targetFilePath;
            return false;
        }

        // 3. 打开文件并读取内容
        QFile file(targetFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "无法打开歌单文件：" << file.errorString();
            return false;
        }
        QByteArray jsonData = file.readAll();
        file.close();

        // 4. 解析JSON文档
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isNull()) {
            qCritical() << "歌单文件JSON格式错误：" << targetFilePath;
            return false;
        }
        QJsonObject sheetObj = doc.object();
        if (sheetObj.isEmpty()) {
            qWarning() << "歌单文件内容为空：" << targetFilePath;
            return false;
        }

        // 5. 调用静态fromJson方法解析为MusicSheet对象
        MusicSheet sheet = AudioUtil::fromJson(sheetObj); // 静态方法，返回MusicSheet

        // 6. 判断解析是否成功（通过关键字段校验）
        // 核心：如果解析失败，返回的sheet.id可能为空或与目标ID不一致
        if (sheet.id.isEmpty()) {
            qCritical() << "歌单解析失败：返回对象ID为空";
            return false;
        }
        if (sheet.id != sheetId) {
            qWarning() << "歌单ID不匹配：文件中为" << sheet.id << "，目标为" << sheetId;
            return false;
        }

        // 7. 额外校验：songCount与songs数组长度是否一致（可选，增强健壮性）
        if (sheet.songCount != sheet.songs.size()) {
            qWarning() << "歌单" << sheetId << "歌曲数量不匹配（songCount="
                       << sheet.songCount << ", 实际歌曲数=" << sheet.songs.size() << "）";
            // 可选：自动修正songCount
            sheet.songCount = sheet.songs.size();
        }

        // 8. 更新到管理器的m_sheets中（覆盖或添加）
        m_sheets.insert(sheetId, sheet);

        qDebug() << "歌单" << sheetId << "从本地加载成功，路径：" << targetFilePath;

        qDebug() << "[Test]: 目前所有的歌单: ";
        for (const auto& sheet : m_sheets)
        {
            qDebug() << sheet.title << '\n';
        }
        return true;
    }

    // 将所有歌单保存到本地文件
    bool saveSheetsToLocal()
    {
        qDebug() << "保存歌单至本地...";
        return false;
    }

    // 将歌单用ID保存进本地文件
    bool saveSheetsToLocalById(const QString& sheetId)
    {
        // 1. 检查歌单ID是否存在
        if (!m_sheets.contains(sheetId)) {
            qWarning() << "歌单ID不存在：" << sheetId;
            return false;
        }

        const MusicSheet& CurrentSheet = getSheetById(sheetId);
        qDebug() << "准备保存的歌单数据：" << CurrentSheet.title << CurrentSheet.songs.size(); // 假设MusicSheet有这些接口
        if (m_sheets.isEmpty()) {
            qWarning() << "无歌单数据可保存";
            return false;
        }

        QJsonObject result = CurrentSheet.toJson();

        if (result.isEmpty())
        {
            qDebug() << "Json体为空";
            return false;
        }

        // 2. 定义单歌单保存路径（例如：原目录/sheets/{sheetId}.json）
        QString sheetDir = QFileInfo(m_localStoragePath).path() + "/SheetsMeta"; // 歌单独立目录
        QString targetFilePath = sheetDir + "/" + sheetId + ".json"; // 目标文件路径
        qDebug() << "实际保存的绝对路径：" << QFileInfo(targetFilePath).absoluteFilePath();


        // 3. 创建歌单目录（确保存在）
        if (!QDir().mkpath(sheetDir)) {
            qCritical() << "无法创建歌单目录：" << sheetDir;
            return false;
        }

        QString tempFilePath = targetFilePath + ".tmp";
        QFile tempFile(tempFilePath);

        // 5.1 打开临时文件
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "无法创建临时文件：" << tempFile.errorString();
            return false;
        }

        // 5.2 写入JSON数据
        QJsonDocument doc(result);
        QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
        if (tempFile.write(jsonData) == -1) {
            qCritical() << "临时文件写入失败：" << tempFile.errorString();
            tempFile.close();
            QFile::remove(tempFilePath);
            return false;
        }
        tempFile.close();

        // 6. 替换目标文件
        qDebug() << "替换目标文件..." << targetFilePath;
        if (QFile::exists(targetFilePath)) {
            if (!QFile::remove(targetFilePath)) {
                qDebug() << "删除旧歌单文件失败：" << targetFilePath;
                QFile::remove(tempFilePath);
                return false;
            }
        }

        if (!QFile::rename(tempFilePath, targetFilePath)) {
            qCritical() << "重命名临时文件失败";
            QFile::remove(tempFilePath);
            return false;
        }

        qDebug() << "歌单" << sheetId << "单独保存成功，路径：" << targetFilePath;
        return true;
    }

    // 保存全部歌单至文件
    bool saveAllSheetsToLocal()
    {
        bool allSuccess = true;
        if (m_sheets.isEmpty())
        {
            qDebug() << "内存里m_sheets内容为空,Save Cancled";
            return false;
        }

        for (const MusicSheet& sheet : m_sheets)
        {
            if (saveSheetsToLocalById(sheet.id))
            {
                qDebug() << "保存成功:" << sheet.id;
            }
            else
            {
                allSuccess = false;
                qDebug() << "保存失败,出现错误:" << sheet.id;
            }
        }

        return allSuccess;
    }

    // get所有引用sheet
    QMap<QString, MusicSheet>& getSheets()
    {
        return m_sheets;
    }
    // 加到内存
    bool addTo_m_sheets(const MusicSheet & favSheet)
    {
        QString sheetId = favSheet.id;

        if (!m_sheets.contains(sheetId))
        {
            m_sheets.insert(sheetId, favSheet);
            return true;
        }
        else
        {
            qDebug() << "Manager里已存在该歌单,id" << sheetId;
            return false;
        }
    }
    // 改内存里Sheet内容ById
    void changeSheetById(const MusicSheet &newSheet,const QString& sheetId)
    {
        if (!m_sheets.contains(sheetId))
        {
            qDebug() << "内存中sheet不存在此ID" << sheetId;
            return;
        }
        qDebug() << "更新中:" << newSheet.id;
        // 获取可修改的歌单引用（非const）
        MusicSheet& currentSheet = m_sheets[sheetId];
        currentSheet.tag = newSheet.tag;
        currentSheet.title = newSheet.title;
        currentSheet.desc = newSheet.desc;
        currentSheet.cover = newSheet.cover;
        currentSheet.coverPath = newSheet.coverPath;
        qDebug() << "更新完成" << currentSheet.id;

        qDebug() << "更新后 - title:" << currentSheet.title << "tag:" << currentSheet.tag;
        qDebug() << "更新完成" << currentSheet.id;
    }

    void loadSheetsFromItemState();

    bool hasSheet(const QString& sheetId) const {
        return m_sheets.contains(sheetId);
    }

    // 添加歌曲到指定歌单
    bool addSongToSheet(const QString& sheetId, const SongMeta& song) {
        if (!hasSheet(sheetId)) {
            qWarning() << "歌单不存在:" << sheetId;
            return false;
        }

        MusicSheet& sheet = m_sheets[sheetId];

        // 检查歌曲是否已存在
        for (const auto& existingSong : sheet.songs) {
            if (existingSong.id == song.id) {
                qDebug() << "歌曲已存在，跳过添加:" << song.name;
                return false;
            }
        }

        // 添加歌曲
        sheet.songs.append(song);
        sheet.songCount = sheet.songs.size();

        qDebug() << "成功添加歌曲到歌单:" << sheetId << "歌曲:" << song.name;
        return true;
    }

    QString getFilePathByIds(const QString & songId,const QString & sheetId)
    {
        if (m_sheets.isEmpty())
        {
            qDebug() << "m_sheets为空";
            return QString("NULL");
        }
        if (!m_sheets.contains(sheetId))
        {
            qDebug() << "m_sheets中未找到 " << sheetId << "的歌单";
            return QString("Err");
        }

        for (const auto & sheet : m_sheets)
        {
            for (const auto & song : sheet.songs)
            {
                if (song.id == songId)
                {
                    qDebug() << "Find TargetID:" << song.id;
                    qDebug() << "[TargetPath]: " << song.filePath;
                    return song.filePath;
                }
            }
        }

        qDebug() << "[TargetSheet]: " << "未找到有效内容";
        return QString("Err");
    }

signals:
    void sheetDataChanged(const QString& sheetId);
    void sheetListChanged();
    void sheetSaved(const QString& sheetId);


private:
    MusicSheetManager() = default; // 私有构造，确保单例
    QMap<QString, MusicSheet> m_sheets; // 存储所有歌单（ID为键）

    // 本地存储文件路径
    QString m_localStoragePath = QString("../LocalSheets/");
public slots:
    void onAddSongToSheet(const QString& sheetId, const SongMeta& song) {
    qDebug() << "MusicSheetManager: 添加歌曲到歌单" << sheetId << "歌曲:" << song.name;

    if (addSongToSheet(sheetId, song)) {
        // 保存到本地文件
        if (saveSheetsToLocalById(sheetId)) {
            qDebug() << "歌曲添加成功并已保存到JSON";
            emit sheetDataChanged(sheetId);
            emit sheetSaved(sheetId);
        } else {
            qWarning() << "歌曲添加成功，但保存到JSON失败";
        }
    }
}
    void onRemoveSongFromSheet(const QString& sheetId, const QString& songId);
};

#endif //RINZEPLAYER_MUSICSHEETMANAGER_H
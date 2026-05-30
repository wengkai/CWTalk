#ifndef CATREADFACTORY_H
#define CATREADFACTORY_H

class QObject;
class ICatReader;
class QString;

namespace CatReaderFactory {

// 根据配置创建 CAT 实例；未启用或不支持时返回 nullptr
ICatReader *create(QObject *parent = nullptr);
ICatReader *createForBackend(const QString &backend, QObject *parent = nullptr);

} // namespace CatReaderFactory

#endif

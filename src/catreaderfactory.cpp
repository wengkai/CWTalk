#include "catreaderfactory.h"
#include "icatreader.h"
#include "yaesucatreader.h"
#include "config.h"

ICatReader *CatReaderFactory::create(QObject *parent)
{
    if (!theConfig.getBool("Cat/Enabled", true))
        return nullptr;

    const QString backend = theConfig.getString("Cat/Backend", "yaesu").toLower();
    if (backend == "yaesu" || backend == "yaesu_ft710" || backend == "ft710")
        return new YaesuCatReader(parent);

    return nullptr;
}

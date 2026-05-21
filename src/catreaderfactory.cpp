#include "catreaderfactory.h"
#include "icatreader.h"
#include "yaesucatreader.h"
#include "icomcatreader.h"
#include "config.h"

ICatReader *CatReaderFactory::create(QObject *parent)
{
    if (!theConfig.getBool("Cat/Enabled", true))
        return nullptr;

    const QString backend = theConfig.getString("Cat/Backend", "yaesu").toLower();
    if (backend == "yaesu" || backend == "yaesu_ft710" || backend == "ft710")
        return new YaesuCatReader(parent);

    if (backend == "icom_ic756pro3" || backend == "icom_ic756proiii"
        || backend == "icom" || backend == "ic756pro3")
        return new IcomCatReader(parent);

    return nullptr;
}

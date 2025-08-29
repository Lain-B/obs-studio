/******************************************************************************
    Copyright (C) 2025 by Taylor Giampaolo <warchamp7@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "display-helpers.hpp"
#include "ThumbnailManager.hpp"
#include <utility/ScreenshotObj.hpp>
#include <widgets/OBSBasic.hpp>

#include <QImageWriter>

#define MIN_THUMBNAIL_UPDATE_INTERVAL_MS 100
#define MIN_SOURCE_UPDATE_INTERVAL_MS 5000

using namespace std::chrono;

QPointer<ThumbnailManager> ThumbnailManager::self;

void ThumbnailItem::imageUpdated(QImage image)
{
	QPixmap newPixmap;
	if (!image.isNull()) {
		newPixmap = QPixmap::fromImage(image);
	}

	pixmap = newPixmap;
	emit updateThumbnail(pixmap);
}

void Thumbnail::thumbnailUpdated(QPixmap pixmap)
{
	emit updateThumbnail(pixmap);
}

ThumbnailManager::ThumbnailManager(QObject *parent) : QObject(parent)
{
	connect(&updateTimer, &QTimer::timeout, this, &ThumbnailManager::updateTick);
}

ThumbnailManager::~ThumbnailManager() {}

QSharedPointer<Thumbnail> ThumbnailManager::getThumbnailInternal(OBSSource source)
{
	std::string uuid = obs_source_get_uuid(source);

	for (auto it = thumbnails.begin(); it != thumbnails.end(); ++it) {
		auto item = it->toStrongRef();
		if (item && item->uuid == uuid) {
			return QSharedPointer<Thumbnail>::create(item);
		}
	}

	QSharedPointer<Thumbnail> thumbnail;
	if ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) != 0) {
		auto item = QSharedPointer<ThumbnailItem>::create(uuid, source);
		thumbnail = QSharedPointer<Thumbnail>::create(item);
		connect(item.get(), &ThumbnailItem::updateThumbnail, thumbnail.get(), &Thumbnail::thumbnailUpdated);

		newThumbnails.push_back(item.toWeakRef());
	}

	updateIntervalChanged(thumbnails.size());
	return thumbnail;
}

QSharedPointer<Thumbnail> ThumbnailManager::getThumbnail(OBSSource source)
{
	if (!self) {
		auto main = OBSBasic::Get();
		if (!main) {
			return QSharedPointer<Thumbnail>();
		}

		self = new ThumbnailManager(OBSBasic::Get());
	}

	return self->getThumbnailInternal(source);
}

bool ThumbnailManager::updatePixmap(QSharedPointer<ThumbnailItem> &sharedPointerItem)
{
	ThumbnailItem *item = sharedPointerItem.get();

	OBSSource source = OBSGetStrongRef(item->weakSource);
	if (!source) {
		return true;
	}

	{
		OBSSource source = OBSGetStrongRef(item->weakSource);
		blog(LOG_DEBUG, "cur number of thumbnails: %d. thumbnail updated: %s", (int)thumbnails.size() + 1,
		     obs_source_get_name(source));
	}

	QPixmap pixmap;
	item->pixmap = pixmap;

	if (source) {
		uint32_t sourceWidth = obs_source_get_width(source);
		uint32_t sourceHeight = obs_source_get_height(source);

		if (sourceWidth == 0 || sourceHeight == 0) {
			return true;
		}

		auto obj = new ScreenshotObj(source);
		obj->setSaveToFile(false);
		obj->setSize(320, 180);

		connect(obj, &ScreenshotObj::imageReady, item, &ThumbnailItem::imageUpdated);
	}

	return true;
}

void ThumbnailManager::updateIntervalChanged(size_t newCount)
{
	int intervalMS = MIN_THUMBNAIL_UPDATE_INTERVAL_MS;
	if (newThumbnails.size() == 0 && newCount > 0) {
		int count = (int)newCount;
		intervalMS = MIN_SOURCE_UPDATE_INTERVAL_MS / count;
		if (intervalMS < MIN_THUMBNAIL_UPDATE_INTERVAL_MS)
			intervalMS = MIN_THUMBNAIL_UPDATE_INTERVAL_MS;
	}

	updateTimer.start(intervalMS);
}

void ThumbnailManager::updateTick()
{
	QSharedPointer<ThumbnailItem> item;
	bool changed = false;
	bool newThumbnail = false;

	while (newThumbnails.size() > 0) {
		changed = true;
		item = newThumbnails.front().toStrongRef();

		newThumbnails.pop_front();
		if (item) {
			newThumbnail = true;
			break;
		}
	}

	if (!item) {
		while (thumbnails.size() > 0) {
			item = thumbnails.front().toStrongRef();
			thumbnails.pop_front();
			if (item) {
				break;
			} else {
				changed = true;
			}
		}
	}
	if (changed && newThumbnails.size() == 0) {
		updateIntervalChanged(thumbnails.size() + (item ? 1 : 0));
	}
	if (!item) {
		return;
	}

	if (updatePixmap(item)) {
		thumbnails.push_back(item.toWeakRef());
	} else {
		thumbnails.push_front(item.toWeakRef());
	}
}

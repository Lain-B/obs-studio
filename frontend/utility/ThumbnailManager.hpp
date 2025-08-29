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

#pragma once

#include <obs.hpp>

#include <QObject>
#include <QPointer>
#include <QPixmap>
#include <QTimer>

#include <deque>

class ThumbnailItem : public QObject {
	Q_OBJECT

	friend class ThumbnailManager;
	friend class Thumbnail;

	std::string uuid;
	OBSWeakSource weakSource;
	QPixmap pixmap;

	void imageUpdated(QImage image);

public:
	inline ThumbnailItem(std::string uuid, OBSSource source) : uuid(uuid), weakSource(OBSGetWeakRef(source)) {}
	inline bool isNull() const { return !weakSource || obs_weak_source_expired(weakSource); }

signals:
	void updateThumbnail(QPixmap pixmap);
};

class Thumbnail : public QObject {
	Q_OBJECT

	friend class ThumbnailManager;

	QSharedPointer<ThumbnailItem> item;

private slots:
	void thumbnailUpdated(QPixmap pixmap);

public:
	inline Thumbnail(QSharedPointer<ThumbnailItem> item) : item(item) {}

	inline QPixmap getPixmap() const { return item->pixmap; }

signals:
	void updateThumbnail(QPixmap pixmap);
};

class ThumbnailManager : public QObject {
	Q_OBJECT

	static QPointer<ThumbnailManager> self;
	QList<QWeakPointer<ThumbnailItem>> newThumbnails;
	QList<QWeakPointer<ThumbnailItem>> thumbnails;
	QTimer updateTimer;

	bool updatePixmap(QSharedPointer<ThumbnailItem> &item);
	void updateTick();

	void updateIntervalChanged(size_t newCount);

	QSharedPointer<Thumbnail> getThumbnailInternal(OBSSource source);

	ThumbnailManager(QObject *parent);

public:
	~ThumbnailManager();

	static QSharedPointer<Thumbnail> getThumbnail(OBSSource source);
};

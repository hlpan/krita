	/*
 *  Copyright (c) 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kis_filter.h"

#include <qlayout.h>
#include <qframe.h>

#include "kis_filter_registry.h"
#include "kis_transaction.h"
#include "kis_undo_adapter.h"
#include "kis_filter_configuration_widget.h"
#include "kis_previewdialog.h"
#include "kis_previewwidget.h"
#include "kis_painter.h"
#include "kis_selection.h"

KisFilter::KisFilter(const QString& name, KisView * view) :
	m_name(name),
	m_view(view),
	m_dialog(0)
{
}

KisFilterConfiguration* KisFilter::configuration(KisFilterConfigurationWidget*)
{
	return 0;
}

void KisFilter::refreshPreview( )
{
	if( m_dialog == 0 )
		return;
	m_dialog -> previewWidget() -> slotRenewLayer();
	KisLayerSP layer = m_dialog -> previewWidget() -> getLayer();
	KisFilterConfiguration* config = configuration(m_widget);
	QRect rect = layer -> extent();
	process((KisPaintDeviceSP) layer, (KisPaintDeviceSP) layer, config, rect);
	m_dialog->previewWidget() -> slotUpdate();
}

KisFilterConfigurationWidget* KisFilter::createConfigurationWidget(QWidget* )
{
	return 0;
}

void KisFilter::slotActivated()
{
	kdDebug() << "Filter activated: " << m_name << "\n";
	KisImageSP img = m_view -> currentImg();
	if (!img) return;

	KisLayerSP layer = img -> activeLayer();
	if (!layer) return;

	// Create the config dialog
	m_dialog = new KisPreviewDialog( (QWidget*) m_view, name().ascii(), true, name());
	m_widget = createConfigurationWidget( (QWidget*)m_dialog->container() );

	if( m_widget != 0)
	{
		m_dialog->previewWidget()->slotSetLayer( layer );
		connect(m_dialog->previewWidget(), SIGNAL(updated()), this, SLOT(refreshPreview()));
		QGridLayout *widgetLayout = new QGridLayout((QWidget *)m_dialog->container(), 1, 1);
		widgetLayout -> addWidget(m_widget, 0 , 0);
		m_dialog->container()->setMinimumSize(m_widget->minimumSize());
		refreshPreview();
		m_dialog -> setFixedSize(m_dialog -> minimumSize());
		if(m_dialog->exec() == QDialog::Rejected )
		{
			delete m_dialog;
			return;
		}
	}

	//Apply the filter
	KisFilterConfiguration* config = configuration(m_widget);

	QRect r1 = layer -> extent();
	//r1.setRect(layer -> getX(), layer -> getY(), r1.width(), r1.height());
	kdDebug() << "Layer rect: x,y: " << r1.x() << ", " << r1.y() << ", W,H: " << r1.width() << ", " << r1.height() << "\n";

	QRect r2 = img -> bounds();
	kdDebug() << "Image rect: x,y: " << r2.x() << ", " << r2.y() << ", W,H: " << r2.width() << ", " << r2.height() << "\n";

	// Filters should work only on the visible part of an image.
	QRect rect = r1.intersect(r2);

	if (layer->hasSelection()) {
		KisSelectionSP s = layer -> selection();
		QRect r3 = s -> selectedRect();
		//r3.setRect(s -> getX(), s -> getY(), r3.width(), r3.height());
		kdDebug() << "Selection rect: x,y: " << r3.x() << ", " << r3.y() << ", W,H: " << r3.width() << ", " << r3.height() << "\n";
		rect = rect.intersect(r3);
	}

	enableProgress();
	kdDebug() << "Going to process filter " << m_name << "\n";
	kdDebug() << "On rect: x,y: " << rect.x() << ", " << rect.y() << ", W,H: " << rect.width() << ", " << rect.height() << "\n";
	
// 	// Always process from the current layer onto a temporary layer
// 	KisPaintDeviceSP dstDev = new KisPaintDevice(layer -> colorStrategy(), "temporary paint device");

	process((KisPaintDeviceSP)layer, (KisPaintDeviceSP)layer, config, rect);

// 	// Blit temporary layer onto the source layer
// 	KisPainter gc(layer);
// 	gc.beginTransaction(name());
// 	gc.bltSelection(rect.x(), rect.y(), COMPOSITE_COPY, dstDev, OPACITY_OPAQUE, 0, 0, rect.width(), rect.height());
// 	img -> undoAdapter() -> addCommand( gc.end() );

	disableProgress();

	img->notify();
	delete m_dialog;
	m_dialog = 0;
	delete config;
}

void KisFilter::enableProgress() {
	m_progressEnabled = true;
}

void KisFilter::disableProgress() {
	m_progressEnabled = false;
}


#include "kis_filter.moc"

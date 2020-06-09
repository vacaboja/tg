/*
 * #include <gtk/gtkHelper.h>
 *
 *  Created on: Oct 24, 2019
 *      Author: jlewis
 *      This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef SRC_GTK_GTKHELPER_H_
#define SRC_GTK_GTKHELPER_H_

#include <gtk/gtk.h>
void setWidgetColor(GtkWidget *widget, cairo_pattern_t *cairocolor);

//check button
GtkWidget *addCheckButton(const char *label,  gboolean bInitialState,	GCallback toggleCallback, gpointer callbackData);
GtkWidget *addCheckButtonBoolean(const char *label, gboolean *iBoolean);

GtkWidget *addSubMenu( const char* label, GtkWidget *parent_menu);
GtkWidget* addMenuItem(const char *label, GtkWidget *parent_menu,
						GCallback activateCallback, gpointer callbackData);
GtkWidget *addMenuCheckButton(const char *label, GtkWidget *parent_menu, gboolean bInitialState,
								GCallback toggleCallback, gpointer callbackData);
void addMenuSeperator(GtkWidget *parent_menu);


//scale , range
GtkWidget *addScaleCallback(const gchar*name, double min, double max, double value, GCallback changedCallback, gpointer callbackData );
GtkWidget *addScale(const gchar*name,  double min, double max, double* value);
void setScaleValue(GtkWidget* hbox, double value);

//spin
GtkWidget * createSpinButtonContainer(const gchar*name,
							 double min, double max,
							 double step, double initValue,
							 GCallback changedCallback, gpointer callbackData
							 );
GtkWidget *getSpinButton(GtkWidget *spinButtonContainer);

#endif /* SRC_GTK_GTKHELPER_H_ */

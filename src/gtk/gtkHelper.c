/*
 * gtkHelper.c
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

#include <gtk/gtk.h>
#include "gtk/gtkHelper.h"


//this is stored here as it issues a compiler warning
void setWidgetColor(GtkWidget *widget, cairo_pattern_t *cairocolor){
	double red,green,blue,alpha;
	cairo_pattern_get_rgba (cairocolor,
	                        &red,&green,&blue,&alpha);
	char rgbaStr[1024];
	sprintf(rgbaStr, "rgba(%f,%f,%f,%f)", 255*red, 255*green, 255*blue, alpha);

	GdkRGBA gdkColor;

	gdk_rgba_parse (&gdkColor, rgbaStr);
	gtk_widget_override_color (widget, GTK_STATE_FLAG_NORMAL, &gdkColor);
}



// menus
GtkWidget *addSubMenu(const char* label, GtkWidget *parent_menu){
	GtkWidget *custom_item = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), custom_item);

	GtkWidget *custom_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM (custom_item), custom_menu);
	return custom_menu;
}

GtkWidget* addMenuItem(const char *label, GtkWidget *parent_menu, GCallback activateCallback, gpointer callbackData ){
	GtkWidget* menuItem = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), menuItem);
	if(activateCallback)
		g_signal_connect(menuItem, "activate", G_CALLBACK(activateCallback), callbackData);
	return menuItem;
}

void addMenuSeperator(GtkWidget *parent_menu){
	gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), gtk_separator_menu_item_new());
}


GtkWidget *addMenuCheckButton(const char *label, GtkWidget *parent_menu, gboolean bInitialState,
	GCallback toggleCallback, gpointer callbackData){

	GtkWidget* chkBtn = gtk_check_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), chkBtn);
	g_signal_connect(chkBtn, "toggled", G_CALLBACK(toggleCallback), callbackData);
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(chkBtn), bInitialState);

	return chkBtn;
}

///   Scale
GtkWidget *addScaleCallback(const gchar*name, double min, double max, double value, GCallback changedCallback, gpointer callbackData ){
	GtkScale* scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
			min, max, 1));
	//gtk_scale_set_draw_value(scale, TRUE);  the default anyway
	gtk_range_set_value (GTK_RANGE(scale), value);
	g_signal_connect(scale, "value_changed", changedCallback, callbackData);
	gtk_widget_show (GTK_WIDGET(scale));
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	if(name!=NULL){
		gtk_widget_set_name(GTK_WIDGET(scale), name);
		GtkWidget *label = gtk_label_new(name);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(scale), TRUE, TRUE, 0);

	return hbox;
}

void setScaleValue(GtkWidget* hbox, double value){
	if(GTK_IS_CONTAINER(hbox)) {
	    GList *children = gtk_container_get_children(GTK_CONTAINER(hbox));
	    GList * child = children;
	    while(child != NULL){
			 if(GTK_IS_RANGE(child->data)){
				 gtk_range_set_value (GTK_RANGE(child->data), value);
			     child = NULL;
			 }else{
				 child = child->next;}
		}

	}

}


static void handle_Range_value_changed(GtkRange *r, gpointer _value){
	double * value = _value;
	*value = gtk_range_get_value (r);
	//debug("Range %s: %lf", (gtk_widget_get_name(GTK_WIDGET(r))), *value);
}

GtkWidget *addScale(const gchar*name, double min, double max, double* value){
	return addScaleCallback(name,  min,  max,  *value, G_CALLBACK(handle_Range_value_changed), value);
}


// spin button

GtkWidget * createSpinButtonContainer(const gchar*name,
							 double min, double max,
							 double step, double initValue,
							 GCallback changedCallback, gpointer callbackData
							 ){
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	GtkWidget * spin_button = gtk_spin_button_new_with_range(min, max, step);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), initValue);
	g_signal_connect(spin_button, "value_changed", G_CALLBACK(changedCallback), callbackData);

	if(name!=NULL){
		gtk_widget_set_name(GTK_WIDGET(spin_button), name);
		GtkWidget *label = gtk_label_new(name);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	}

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(spin_button), TRUE, TRUE, 0);

	return hbox;
}

GtkWidget *getSpinButton(GtkWidget *spinButtonContainer){
	if(GTK_IS_SPIN_BUTTON(spinButtonContainer)) //in case of confusion
		return spinButtonContainer;
	if(GTK_IS_CONTAINER(spinButtonContainer)) {
		GList *children = gtk_container_get_children(GTK_CONTAINER(spinButtonContainer));
		GList * child = children;
		while(child != NULL){
			 if(GTK_IS_SPIN_BUTTON(child->data)){
				 return child->data;
			 }else{
				 child = child->next;}
		}
	}

	return NULL;
}

//checkbuttons
GtkWidget *addCheckButton(const char *label,  gboolean bInitialState,
									GCallback toggleCallback, gpointer callbackData){

	GtkWidget* chkBtn = gtk_check_button_new_with_label(label);
	g_signal_connect(chkBtn, "toggled", G_CALLBACK(toggleCallback), callbackData);
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(chkBtn), bInitialState);
	return chkBtn;
}

static void handle_BooleanToggle( GtkToggleButton *b, gboolean *iBoolean){
	*iBoolean = gtk_toggle_button_get_active(b);

}


GtkWidget *addCheckButtonBoolean(const char *label, gboolean *iBoolean){

	return addCheckButton(label, *iBoolean,	G_CALLBACK(handle_BooleanToggle), (gpointer) iBoolean);
}



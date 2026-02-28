#ifndef MENU_H
#define MENU_H

#include "hci.h"

extern hciRemoteDevice *m_selected_device;

void menu(hci_context_t *context);
void hci_startInquiry(hci_context_t *context, unsigned nSeconds);
hciRemoteDevice *selectDevice_by_addr(const char *mac_addr);
void selectDevice(hci_context_t *context, u8 num);

#endif // MENU_H

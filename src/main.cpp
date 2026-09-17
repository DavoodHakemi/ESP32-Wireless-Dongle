#include "application/DongleApplication.h"

dongle::application::DongleApplication application;

void setup() {
    application.begin();
}
void loop() {
    application.update();
}

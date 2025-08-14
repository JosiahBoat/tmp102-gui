SUMMARY = "TMP102 Temperature Display GUI"
DESCRIPTION = "GUI program to display temperature from TMP102 FIFO"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://temperature_gui.c"

S = "${WORKDIR}"

DEPENDS = "gtk+3"

do_compile() {
    ${CC} ${CFLAGS} `pkg-config --cflags gtk+-3.0` temperature_gui.c -o temperature_gui `pkg-config --libs gtk+-3.0`
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 temperature_gui ${D}${bindir}
}

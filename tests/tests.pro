TEMPLATE = subdirs
SUBDIRS = ut_categorydefinitionstore ut_notification ut_notificationmanager ut_notificationlistmodel ut_lipstickdbusinterface ut_lipsticksettings ut_notificationpreviewpresenter

support_files.commands += $$PWD/gen-tests-xml.sh > $$OUT_PWD/tests.xml
support_files.target = support_files
support_files.files += $$OUT_PWD/tests.xml
support_files.path = /opt/tests/lipstick-tests/test-definition
support_files.CONFIG += no_check_exist

INSTALLS += support_files

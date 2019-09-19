TEMPLATE = subdirs

SUBDIRS += RdpBrowser RdpProxy

RdpBrowser.depends = RdpProxy

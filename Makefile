CC := g++
SRC := evince-restorer
TEMPLATE := evince-restorer.template
DESKTOP := ~/.local/share/applications/evince-restorer.desktop

$(SRC): $(SRC).cpp
	$(CC) -pthread -o $@ $<

install:
	cp $(TEMPLATE) $(DESKTOP)
	echo "Icon=`pwd`/evince-restorer.png" >> $(DESKTOP)
	echo "Exec=bash -c \"cd `pwd` && `pwd`/$(SRC) >> /tmp/evince-restorer.log\"" >> $(DESKTOP)
	sudo chown ${USER}:${USER} $(DESKTOP)
	chmod +x $(DESKTOP)

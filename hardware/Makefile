PARTFILES=\
	  part.BluePillMerge_a126641d7fb79d6ad0f204e9abc523f4_5.fzp \
	  svg.breadboard.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_breadboard.svg \
	  svg.icon.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_icon.svg \
	  svg.pcb.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_pcb.svg \
	  svg.schematic.BluePillMerge_a9966dcd0ea15e21ad555ae4bf4af3e4_1_schematic.svg

.PHONY: all clean prepare

all: sunhid.fzz

clean:
	rm -f sunhid.fzz

%.fzz: %.fz $(PARTFILES)
	zip $@ $?

prepare:
	unzip -o sunhid.fzz
	perl cleanup_pre_commit.pl sunhid.fz > sunhid.fz.new
	mv -f sunhid.fz.new sunhid.fz


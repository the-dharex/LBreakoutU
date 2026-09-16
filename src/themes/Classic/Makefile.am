EXTRA_DIST = ABOUT fsmall.otf fnormal.otf theme.ini SIL-OFL.txt \
back0.png back1.png back2.png back3.png back4.png back5.png \
ball.png bricks.png explosions_dark.png explosions.png \
extras.png f_frame.png f_game.png fr_left.png fr_right.png \
fr_top.png f_small_red.png f_small_white.png f_small_yellow.png \
f_tiny_black.png f_white.png f_yellow.png life.png cursor.png \
menuback.jpg paddle.png shine.png shot.png warp.png weapon.png \
attach.wav       energyhit.wav  goldshower.wav     shot.wav \
bonusmagnet.wav  excellent.wav  joker.wav          shrink.wav \
brickhit.wav     expand.wav     looselife.wav      speeddown.wav \
chaos.wav        explball.wav   malusmagnet.wav    speedup.wav \
click.wav        explosion.wav  menuclick.wav      standard.wav \
dammit.wav       extraball.wav  menumotion.wav     timeadd.wav \
damn.wav         freeze.wav     reflectbrick.wav   verygood.wav \
darkness.wav     gainlife.wav   reflectpaddle.wav  wall.wav \
disable.wav      ghost.wav      score.wav          weakball.wav

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datdir)/themes/Classic
	@for file in $(EXTRA_DIST); do\
	  $(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(datdir)/themes/Classic/$$file;\
	done


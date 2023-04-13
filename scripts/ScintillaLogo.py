# ScintillaLogo.py
# Requires Python 3.6.
# Requires Pillow https://python-pillow.org/, tested with 7.2.0 on Windows 10
# pip3 install pillow
# Requires aggdraw: pip3 install aggdraw
# Requires nasalization font

import random
from PIL import Image, ImageDraw, ImageFont
import aggdraw

colours = [
(136,0,21,255),
(237,28,36,255),
(255,127,39,255),
(255,201,14,255),
(185,122,87,255),
(255,174,201,255),
(181,230,29,255),
(34,177,76,255),
(153,217,234,255),
(0,162,232,255),
(112,146,190,255),
(63,72,204,255),
(200,191,231,255),
]

width = 1280
height = 150

def drawCircles(dr):
	for y in range(0,height, 2):
		x = 0
		while x < width:
			#lexeme = random.randint(2, 20)
			lexeme = random.expovariate(0.30)
			colour = random.choice(colours)
			ellipse = (x, y, x+lexeme, y+lexeme)
			brush = aggdraw.Brush(colour, 255)
			dr.ellipse(ellipse, brush, brush)
			x += lexeme + 3

def drawBlocks(dr):
	coloursWithDark = colours
	y = height
	x = -100
	while x < width:
		colour = random.choice(coloursWithDark)
		strokeRectangle = (x+0.25, y+0.25, x+0.25+150, y+0.25-150)
		brush = aggdraw.Pen(colour, 2.0)
		dr.line(strokeRectangle, brush)
		x += 4

def drawCirclesDark(dr):
	for y in range(0,height, 2):
		x = 0
		while x < width:
			lexeme = random.expovariate(0.3)
			light = int(random.uniform(1,50))
			colour = (light, light, light, 255)
			ellipse = (x, y, x+lexeme, y+lexeme)
			brush = aggdraw.Brush(colour)
			dr.ellipse(ellipse, brush, brush)
			x += lexeme + 3

def drawLogoScintilla():
	# Ensure same image each time
	random.seed(3)

	font = ImageFont.truetype(font="nasalization-rg.ttf", size=190)

	imageMask = Image.new("L", (width, height), color=(0xff))
	drMask = ImageDraw.Draw(imageMask)
	drMask.text((30, -29), "Scintilla", font=font, fill=(0))

	imageBack = Image.new("RGBA", (width, height), color=(0,0,0))
	drBack = aggdraw.Draw(imageBack)
	drawCirclesDark(drBack)
	drBack.flush()

	imageLines = Image.new("RGBA", (width, height), color=(0,0,0))
	dr = aggdraw.Draw(imageLines)
	drawBlocks(dr)
	dr.flush()

	imageOut = Image.composite(imageBack, imageLines, imageMask)

	imageOut.save("../../scintilla/doc/ScintillaLogo.png", "png")

	imageDoubled = imageOut.resize((width*2, height * 2), Image.Resampling.NEAREST)

	imageDoubled.save("../../scintilla/doc/ScintillaLogo2x.png", "png")

def drawLogoSciTE():
	# Ensure same image each time
	random.seed(4)

	font = ImageFont.truetype(font="nasalization-rg.ttf", size=190)

	imageMask = Image.new("L", (width, height), color=(0xff))
	drMask = ImageDraw.Draw(imageMask)
	drMask.text((30, -29), "SciTE", font=font, fill=(0))

	imageBack = Image.new("RGBA", (width, height), color=(0,0,0))
	drBack = aggdraw.Draw(imageBack)
	drawCirclesDark(drBack)
	drBack.flush()

	imageLines = Image.new("RGBA", (width, height), color=(0,0,0))
	dr = aggdraw.Draw(imageLines)
	drawCircles(dr)
	dr.flush()

	imageOut = Image.composite(imageBack, imageLines, imageMask)

	imageOut.save("../doc/SciTELogo.png", "png")

	imageDoubled = imageOut.resize((width*2, height * 2), Image.Resampling.NEAREST)

	imageDoubled.save("../doc/SciTELogo2x.png", "png")

drawLogoSciTE()
drawLogoScintilla()

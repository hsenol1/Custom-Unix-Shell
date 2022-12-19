#! pip3 install openai
import os
import openai
import sys
import urllib.request
from PIL import Image as Imagepil

secret = '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'

def main():
	if(len(sys.argv)!=2):
		return -1;

	inp = sys.argv[1].replace("-"," ")
	openai.api_key = secret

	image = openai.Image.create(
		prompt = inp,
		n = 1,
		size = "1024x1024"
		)
	image_url = image['data'][0]['url']

	urllib.request.urlretrieve(image_url, "openai.png")
	img = Imagepil.open("openai.png")

	img.show()


if __name__ == "__main__":
	main()
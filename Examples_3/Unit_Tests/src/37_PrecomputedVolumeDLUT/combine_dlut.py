from PIL import Image, ImageOps

dlut0 = Image.open('dlut_0.png').split()
dlut90 = Image.open('dlut_90.png').split()
dlut180 = Image.open('dlut_180.png').split()
dlut270 = Image.open('dlut_270.png').split()

zero = Image.new('RGB', dlut0[0].size).split()[0]

dlut0 = Image.composite(dlut0[0], zero, dlut0[3])
dlut90 = Image.composite(dlut90[0], zero, dlut90[3])
dlut180 = Image.composite(dlut180[0], zero, dlut180[3])
dlut270 = Image.composite(dlut270[0], zero, dlut270[3])

dlut = Image.merge('RGBA', (dlut0, dlut90, dlut180, dlut270))
dlut.save('dlut.png')

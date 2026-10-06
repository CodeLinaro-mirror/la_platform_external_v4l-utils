#include <endian.h>

#include "v4l2-ctl.h"

static struct v4l2_frmsizeenum frmsize; /* list frame sizes */
static struct v4l2_frmivalenum frmival; /* list frame intervals */
static video_format_request format_request;
static unsigned mbus_code;
static bool enum_all;

void vidcap_usage()
{
	printf("\nVideo Capture Formats options:\n"
	       "  --list-formats [<mbus_code>|all]\n"
	       "		     display supported video formats. <mbus_code> is an optional\n"
	       "		     media bus code, if the device has capability V4L2_CAP_IO_MC\n"
	       "		     then only formats that support this media bus code are listed.\n"
	       "		     When 'all' is specified it enumerates all pixel formats if\n"
	       "		     V4L2_FMTDESC_FLAG_ENUM_ALL flag is supported by the driver.\n"
	       "		     [VIDIOC_ENUM_FMT]\n"
	       "  --list-formats-ext [<mbus_code>|all]\n"
	       "		     display supported video formats including frame sizes and intervals\n"
	       "		     <mbus_code> is an optional media bus code, if the device has\n"
	       "		     capability V4L2_CAP_IO_MC then only formats that support this\n"
	       "		     media bus code are listed.\n"
	       "		     When 'all' is specified it enumerates all pixel formats if\n"
	       "		     V4L2_FMTDESC_FLAG_ENUM_ALL flag is supported by the driver.\n"
	       "		     [VIDIOC_ENUM_FMT]\n"
	       "  --list-framesizes <f>\n"
	       "                     list supported framesizes for pixelformat <f>\n"
	       "                     [VIDIOC_ENUM_FRAMESIZES]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-frameintervals width=<w>,height=<h>,pixelformat=<f>\n"
	       "                     list supported frame intervals for pixelformat <f> and\n"
	       "                     the given width and height [VIDIOC_ENUM_FRAMEINTERVALS]\n"
	       "                     pixelformat is the fourcc value as a string\n"
	       "  --list-fields      list supported fields for the current format\n"
	       "  -V, --get-fmt-video\n"
	       "     		     query the video capture format [VIDIOC_G_FMT]\n"
	       "  -v, --set-fmt-video\n"
	       "  --try-fmt-video width=<w>,height=<h>,pixelformat=<pf>,field=<f>,colorspace=<c>,\n"
	       "                  xfer=<xf>,ycbcr=<y>,hsv=<hsv>,quantization=<q>,\n"
	       "                  premul-alpha=<0/1>,bytesperline=<bpl>,sizeimage=<sz>\n"
	       "                     set/try the video capture format [VIDIOC_S/TRY_FMT]\n"
	       "                     pixelformat is either the format index as reported by\n"
	       "                       --list-formats, or the fourcc value as a string.\n"
	       "                     The bytesperline and sizeimage options can be used multiple times,\n"
	       "                       once for each plane.\n"
	       "                     premul-alpha sets (1) or clears (0) V4L2_PIX_FMT_FLAG_PREMUL_ALPHA.\n"
	       "                     <f> can be one of the following field layouts:\n"
	       "                       any, none, top, bottom, interlaced, seq_tb, seq_bt,\n"
	       "                       alternate, interlaced_tb, interlaced_bt\n"
	       "                     <c> can be one of the following colorspaces:\n"
	       "                       smpte170m, smpte240m, rec709, 470m, 470bg, jpeg, srgb,\n"
	       "                       oprgb, bt2020, dcip3\n"
	       "                     <xf> can be one of the following transfer functions:\n"
	       "                       default, 709, srgb, oprgb, smpte240m, smpte2084, dcip3, none\n"
	       "                     <y> can be one of the following Y'CbCr encodings:\n"
	       "                       default, 601, 709, xv601, xv709, bt2020, bt2020c, smpte240m\n"
	       "                     <hsv> can be one of the following HSV encodings:\n"
	       "                       default, 180, 256\n"
	       "                     <q> can be one of the following quantization methods:\n"
	       "                       default, full-range, lim-range\n"
	       );
}

static void print_video_fields(int fd)
{
	struct v4l2_format fmt;
	struct v4l2_format tmp;

	memset(&fmt, 0, sizeof(fmt));
	fmt.fmt.pix.priv = priv_magic;
	fmt.type = vidcap_buftype;
	if (test_ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
		return;

	printf("Supported Video Fields:\n");
	for (__u32 f = V4L2_FIELD_NONE; f <= V4L2_FIELD_INTERLACED_BT; f++) {
		bool ok;

		tmp = fmt;
		if (is_multiplanar)
			tmp.fmt.pix_mp.field = f;
		else
			tmp.fmt.pix.field = f;
		if (test_ioctl(fd, VIDIOC_TRY_FMT, &tmp) < 0)
			continue;
		if (is_multiplanar)
			ok = tmp.fmt.pix_mp.field == f;
		else
			ok = tmp.fmt.pix.field == f;
		if (ok)
			printf("\t%s\n", field2s(f).c_str());
	}
}

void vidcap_cmd(int ch, char *optarg)
{
	char *value, *subs;

	switch (ch) {
	case OptSetVideoFormat:
	case OptTryVideoFormat:
		if (!parse_fmt(optarg, format_request)) {
			vidcap_usage();
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptListFormats:
	case OptListFormatsExt:
		if (optarg) {
			if (strstr(optarg , "all"))
				enum_all = true;
			else
				mbus_code = strtoul(optarg, nullptr, 0);
		}
		break;
	case OptListFrameSizes:
		frmsize.pixel_format = parse_pixelformat(optarg);
		break;
	case OptListFrameIntervals:
		subs = optarg;
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"width",
				"height",
				"pixelformat",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				frmival.width = strtoul(value, nullptr, 0);
				break;
			case 1:
				frmival.height = strtoul(value, nullptr, 0);
				break;
			case 2:
				frmival.pixel_format = parse_pixelformat(value);
				break;
			default:
				vidcap_usage();
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	}
}

int vidcap_get_and_update_fmt(cv4l_fd &_fd, struct v4l2_format &vfmt)
{
	return video_get_and_update_fmt(_fd, vfmt, vidcap_buftype,
				       priv_magic, format_request);
}

void vidcap_set(cv4l_fd &_fd)
{
	if (options[OptSetVideoFormat] || options[OptTryVideoFormat]) {
		int fd = _fd.g_fd();
		int ret;
		struct v4l2_format vfmt;

		if (vidcap_get_and_update_fmt(_fd, vfmt) == 0) {
			if (options[OptSetVideoFormat])
				ret = doioctl(fd, VIDIOC_S_FMT, &vfmt);
			else
				ret = doioctl(fd, VIDIOC_TRY_FMT, &vfmt);
			if (ret == 0 && (verbose || options[OptTryVideoFormat]))
				printfmt(fd, vfmt);
		}
	}
}

void vidcap_get(cv4l_fd &fd)
{
	if (options[OptGetVideoFormat]) {
		struct v4l2_format vfmt;

		memset(&vfmt, 0, sizeof(vfmt));
		vfmt.fmt.pix.priv = priv_magic;
		vfmt.type = vidcap_buftype;
		if (doioctl(fd.g_fd(), VIDIOC_G_FMT, &vfmt) == 0)
			printfmt(fd.g_fd(), vfmt);
	}
}

void vidcap_list(cv4l_fd &fd)
{
	if (options[OptListFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, vidcap_buftype, mbus_code, enum_all);
	}

	if (options[OptListFormatsExt]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats_ext(fd, vidcap_buftype, mbus_code, enum_all);
	}

	if (options[OptListFields]) {
		print_video_fields(fd.g_fd());
	}

	if (options[OptListFrameSizes]) {
		frmsize.index = 0;
		if (frmsize.pixel_format < 256) {
			frmsize.pixel_format =
				find_pixel_format(fd.g_fd(), frmsize.pixel_format,
						  false, is_multiplanar);
			if (!frmsize.pixel_format) {
				fprintf(stderr, "The pixelformat index was invalid\n");
				std::exit(EXIT_FAILURE);
			}
		}
		if (!valid_pixel_format(fd.g_fd(), frmsize.pixel_format, false, is_multiplanar) &&
		    !valid_pixel_format(fd.g_fd(), frmsize.pixel_format, true, is_multiplanar)) {
			fprintf(stderr, "The pixelformat '%s' is invalid\n",
				fcc2s(frmsize.pixel_format).c_str());
			std::exit(EXIT_FAILURE);
		}

		printf("ioctl: VIDIOC_ENUM_FRAMESIZES\n");
		while (test_ioctl(fd.g_fd(), VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			print_frmsize(frmsize, "");
			frmsize.index++;
		}
	}

	if (options[OptListFrameIntervals]) {
		frmival.index = 0;
		if (frmival.pixel_format < 256) {
			frmival.pixel_format =
				find_pixel_format(fd.g_fd(), frmival.pixel_format,
						  false, is_multiplanar);
			if (!frmival.pixel_format) {
				fprintf(stderr, "The pixelformat index was invalid\n");
				std::exit(EXIT_FAILURE);
			}
		}
		if (!valid_pixel_format(fd.g_fd(), frmival.pixel_format, false, is_multiplanar)) {
			fprintf(stderr, "The pixelformat '%s' is invalid\n",
				fcc2s(frmival.pixel_format).c_str());
			std::exit(EXIT_FAILURE);
		}

		printf("ioctl: VIDIOC_ENUM_FRAMEINTERVALS\n");
		while (test_ioctl(fd.g_fd(), VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
			print_frmival(frmival, "");
			frmival.index++;
		}
	}
}

void print_touch_buffer(FILE *f, cv4l_buffer &buf, cv4l_fmt &fmt, cv4l_queue &q)
{
	static constexpr char img[16] = {
		'.', ',', ':', ';', '!', '|', 'i', 'c',
		'n', 'o', 'm', 'I', 'C', 'N', 'O', 'M',
	};
	auto vbuf = static_cast<__s16 *>(q.g_dataptr(buf.g_index(), 0));
	__u32 x, y;

	switch (fmt.g_pixelformat()) {
	case V4L2_TCH_FMT_DELTA_TD16:
		for (y = 0; y < fmt.g_height(); y++) {
			fprintf(f, "TD16: ");

			for (x = 0; x < fmt.g_width(); x++, vbuf++) {
				auto v = static_cast<__s16>(le16toh(*vbuf));

				if (!options[OptConcise])
					fprintf(f, "% 4d", v);
				else if (v > 255)
					fprintf(f, "*");
				else if (v < -32)
					fprintf(f, "-");
				else if (v < 0)
					fprintf(f, "%c", img[0]);
				else
					fprintf(f, "%c", img[v / 16]);
			}
			fprintf(f, "\n");
		}
		break;
	}
}

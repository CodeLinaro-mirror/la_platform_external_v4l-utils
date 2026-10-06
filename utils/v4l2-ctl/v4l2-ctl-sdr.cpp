#include "v4l2-ctl.h"

static struct v4l2_format vfmt;	/* set_format/get_format */

void sdr_usage()
{
	printf("\nSDR Formats options:\n"
	       "  --list-formats-sdr display supported SDR capture formats [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-sdr      query the SDR capture format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-sdr <f>  set the SDR capture format [VIDIOC_S_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-cap, or the fourcc value as a string\n"
	       "  --try-fmt-sdr <f>  try the SDR capture format [VIDIOC_TRY_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-cap, or the fourcc value as a string\n"
	       "  --list-formats-sdr-out\n"
	       "                     display supported SDR output formats [VIDIOC_ENUM_FMT]\n"
	       "  --get-fmt-sdr-out  query the SDR output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-sdr-out <f>\n"
	       "                     set the SDR output format [VIDIOC_S_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-out, or the fourcc value as a string\n"
	       "  --try-fmt-sdr-out <f>\n"
	       "                     try the SDR output format [VIDIOC_TRY_FMT]\n"
	       "                     parameter is either the format index as reported by\n"
	       "                     --list-formats-sdr-out, or the fourcc value as a string\n"
	       );
}

void sdr_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptSetSdrFormat:
	case OptTrySdrFormat:
	case OptSetSdrOutFormat:
	case OptTrySdrOutFormat:
		if (strlen(optarg) == 0) {
			sdr_usage();
			std::exit(EXIT_FAILURE);
		} else if (strlen(optarg) == 4) {
			vfmt.fmt.sdr.pixelformat = parse_pixelformat(optarg);
		} else {
			vfmt.fmt.sdr.pixelformat = strtoul(optarg, nullptr, 0);
		}
		break;
	}
}

static void __sdr_set(cv4l_fd &_fd, bool set, bool _try, __u32 type)
{
	struct v4l2_format in_vfmt;
	int fd = _fd.g_fd();
	int ret;

	if (!set && !_try)
		return;

	in_vfmt.type = type;
	in_vfmt.fmt.sdr.pixelformat = vfmt.fmt.sdr.pixelformat;

	if (in_vfmt.fmt.sdr.pixelformat < 256) {
		struct v4l2_fmtdesc fmt = {};

		fmt.index = in_vfmt.fmt.sdr.pixelformat;
		fmt.type = in_vfmt.type;

		if (doioctl(fd, VIDIOC_ENUM_FMT, &fmt))
			fmt.pixelformat = 0;

		in_vfmt.fmt.sdr.pixelformat = fmt.pixelformat;
	}

	if (set)
		ret = doioctl(fd, VIDIOC_S_FMT, &in_vfmt);
	else
		ret = doioctl(fd, VIDIOC_TRY_FMT, &in_vfmt);
	if (ret == 0 && (verbose || _try))
		printfmt(fd, in_vfmt);
}

void sdr_set(cv4l_fd &_fd)
{
	__sdr_set(_fd, options[OptSetSdrFormat], options[OptTrySdrFormat],
		  V4L2_BUF_TYPE_SDR_CAPTURE);
	__sdr_set(_fd, options[OptSetSdrOutFormat],
		  options[OptTrySdrOutFormat], V4L2_BUF_TYPE_SDR_OUTPUT);
}

static void __sdr_get(cv4l_fd &fd, __u32 type)
{
	vfmt.type = type;
	if (doioctl(fd.g_fd(), VIDIOC_G_FMT, &vfmt) == 0)
		printfmt(fd.g_fd(), vfmt);
}

void sdr_get(cv4l_fd &fd)
{
	if (options[OptGetSdrFormat])
		__sdr_get(fd, V4L2_BUF_TYPE_SDR_CAPTURE);
	if (options[OptGetSdrOutFormat])
		__sdr_get(fd, V4L2_BUF_TYPE_SDR_OUTPUT);
}

void sdr_list(cv4l_fd &fd)
{
	if (options[OptListSdrFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_SDR_CAPTURE, 0, false);
	}
	if (options[OptListSdrOutFormats]) {
		printf("ioctl: VIDIOC_ENUM_FMT\n");
		print_video_formats(fd, V4L2_BUF_TYPE_SDR_OUTPUT, 0, false);
	}
}

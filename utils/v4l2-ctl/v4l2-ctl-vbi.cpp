#include <cstring>

#include "compiler.h"
#include "v4l2-ctl.h"

static struct v4l2_format sliced_fmt;	  /* set_format/get_format for sliced VBI */
static struct v4l2_format sliced_fmt_out; /* set_format/get_format for sliced VBI output */
static struct v4l2_format raw_fmt;	  /* set_format/get_format for VBI */
static struct v4l2_format raw_fmt_out;	  /* set_format/get_format for VBI output */

void vbi_usage()
{
	printf("\nVBI Formats options:\n"
	       "  --get-sliced-vbi-cap\n"
	       "		     query the sliced VBI capture capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  --get-sliced-vbi-out-cap\n"
	       "		     query the sliced VBI output capabilities\n"
	       "                     [VIDIOC_G_SLICED_VBI_CAP]\n"
	       "  -B, --get-fmt-sliced-vbi\n"
	       "		     query the sliced VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-sliced-vbi-out\n"
	       "		     query the sliced VBI output format [VIDIOC_G_FMT]\n"
	       "  -b, --set-fmt-sliced-vbi\n"
	       "  --try-fmt-sliced-vbi\n"
	       "  --set-fmt-sliced-vbi-out\n"
	       "  --try-fmt-sliced-vbi-out <mode>\n"
	       "                     set/try the sliced VBI capture/output format to <mode>\n"
	       "                     [VIDIOC_S/TRY_FMT], <mode> is a comma separated list of:\n"
	       "                     off:      turn off sliced VBI (cannot be combined with\n"
	       "                               other modes)\n"
	       "                     teletext: teletext (PAL/SECAM)\n"
	       "                     cc:       closed caption (NTSC)\n"
	       "                     wss:      widescreen signal (PAL/SECAM)\n"
	       "                     vps:      VPS (PAL/SECAM)\n"
	       "  --get-fmt-vbi      query the VBI capture format [VIDIOC_G_FMT]\n"
	       "  --get-fmt-vbi-out  query the VBI output format [VIDIOC_G_FMT]\n"
	       "  --set-fmt-vbi\n"
	       "  --try-fmt-vbi\n"
	       "  --set-fmt-vbi-out\n"
	       "  --try-fmt-vbi-out samplingrate=<r>,offset=<o>,samplesperline=<spl>,\n"
	       "                     start0=<s0>,count0=<c0>,start1=<s1>,count1=<c1>\n"
	       "                     set/try the raw VBI capture/output format [VIDIOC_S/TRY_FMT]\n"
	       "                     samplingrate: samples per second\n"
	       "                     offset: horizontal offset in samples\n"
	       "                     samplesperline: samples per line\n"
	       "                     start0: start line number of the first field\n"
	       "                     count0: number of lines in the first field\n"
	       "                     start1: start line number of the second field\n"
	       "                     count1: number of lines in the second field\n"
	       );
}

static void print_sliced_vbi_cap(struct v4l2_sliced_vbi_cap &cap)
{
	printf("\tType           : %s\n", buftype2s(cap.type).c_str());
	printf("\tService Set    : %s\n",
			service2s(cap.service_set).c_str());
	for (int i = 0; i < 24; i++) {
		printf("\tService Line %2d: %8s / %-8s\n", i,
				service2s(cap.service_lines[0][i]).c_str(),
				service2s(cap.service_lines[1][i]).c_str());
	}
}

void vbi_cmd(int ch, char *optarg)
{
	char *value, *subs;
	bool found_off = false;
	v4l2_format *sliced = &sliced_fmt;
	v4l2_format *raw = &raw_fmt;

	switch (ch) {
	case OptSetSlicedVbiOutFormat:
	case OptTrySlicedVbiOutFormat:
		sliced = &sliced_fmt_out;
		fallthrough;
	case OptSetSlicedVbiFormat:
	case OptTrySlicedVbiFormat:
		sliced->fmt.sliced.service_set = 0;
		if (optarg[0] == 0) {
			fprintf(stderr, "empty string\n");
			vbi_usage();
			std::exit(EXIT_FAILURE);
		}
		while (*optarg) {
			subs = std::strchr(optarg, ',');
			if (subs)
				*subs = 0;

			if (!strcmp(optarg, "off"))
				found_off = true;
			else if (!strcmp(optarg, "teletext"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_TELETEXT_B;
			else if (!strcmp(optarg, "cc"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_CAPTION_525;
			else if (!strcmp(optarg, "wss"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_WSS_625;
			else if (!strcmp(optarg, "vps"))
				sliced->fmt.sliced.service_set |=
					V4L2_SLICED_VPS;
			else
				vbi_usage();
			if (subs == nullptr)
				break;
			optarg = subs + 1;
		}
		if (found_off && sliced->fmt.sliced.service_set) {
			fprintf(stderr, "Sliced VBI mode 'off' cannot be combined with other modes\n");
			vbi_usage();
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptSetVbiOutFormat:
	case OptTryVbiOutFormat:
		raw = &raw_fmt_out;
		fallthrough;
	case OptSetVbiFormat:
	case OptTryVbiFormat:
		subs = optarg;
		memset(&raw->fmt.vbi, 0, sizeof(raw->fmt.vbi));
		while (*subs != '\0') {
			static constexpr const char *subopts[] = {
				"samplingrate",
				"offset",
				"samplesperline",
				"start0",
				"start1",
				"count0",
				"count1",
				nullptr
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				raw->fmt.vbi.sampling_rate = strtoul(value, nullptr, 0);
				break;
			case 1:
				raw->fmt.vbi.offset = strtoul(value, nullptr, 0);
				break;
			case 2:
				raw->fmt.vbi.samples_per_line = strtoul(value, nullptr, 0);
				break;
			case 3:
				raw->fmt.vbi.start[0] = strtoul(value, nullptr, 0);
				break;
			case 4:
				raw->fmt.vbi.start[1] = strtoul(value, nullptr, 0);
				break;
			case 5:
				raw->fmt.vbi.count[0] = strtoul(value, nullptr, 0);
				break;
			case 6:
				raw->fmt.vbi.count[1] = strtoul(value, nullptr, 0);
				break;
			default:
				vbi_usage();
				break;
			}
		}
		break;
	}
}

static void fill_raw_vbi(v4l2_vbi_format &dst, const v4l2_vbi_format &src)
{
	if (src.sampling_rate)
		dst.sampling_rate = src.sampling_rate;
	if (src.offset)
		dst.offset = src.offset;
	if (src.samples_per_line)
		dst.samples_per_line = src.samples_per_line;
	if (src.start[0])
		dst.start[0] = src.start[0];
	if (src.start[1])
		dst.start[1] = src.start[1];
	if (src.count[0])
		dst.count[0] = src.count[0];
	if (src.count[1])
		dst.count[1] = src.count[1];
}

static void __vbi_set_sliced(cv4l_fd &_fd, bool set, bool _try, __u32 type,
			     v4l2_format &sliced)
{
	int fd = _fd.g_fd();
	int ret;

	if (!set && !_try)
		return;

	sliced.type = type;
	if (set)
		ret = doioctl(fd, VIDIOC_S_FMT, &sliced);
	else
		ret = doioctl(fd, VIDIOC_TRY_FMT, &sliced);
	if (ret == 0 && (verbose || _try))
		printfmt(fd, sliced);
}

static void __vbi_set_raw(cv4l_fd &_fd, bool set, bool _try, __u32 type,
			  const v4l2_format &raw)
{
	int fd = _fd.g_fd();
	v4l2_format fmt;
	int ret;

	if (!set && !_try)
		return;

	fmt.type = type;
	doioctl(fd, VIDIOC_G_FMT, &fmt);
	fill_raw_vbi(fmt.fmt.vbi, raw.fmt.vbi);
	if (set)
		ret = doioctl(fd, VIDIOC_S_FMT, &fmt);
	else
		ret = doioctl(fd, VIDIOC_TRY_FMT, &fmt);
	if (ret == 0 && (verbose || _try))
		printfmt(fd, fmt);
}

void vbi_set(cv4l_fd &_fd)
{
	__vbi_set_sliced(_fd, options[OptSetSlicedVbiFormat],
			 options[OptTrySlicedVbiFormat],
			 V4L2_BUF_TYPE_SLICED_VBI_CAPTURE, sliced_fmt);
	__vbi_set_sliced(_fd, options[OptSetSlicedVbiOutFormat],
			 options[OptTrySlicedVbiOutFormat],
			 V4L2_BUF_TYPE_SLICED_VBI_OUTPUT, sliced_fmt_out);
	__vbi_set_raw(_fd, options[OptSetVbiFormat], options[OptTryVbiFormat],
		      V4L2_BUF_TYPE_VBI_CAPTURE, raw_fmt);
	__vbi_set_raw(_fd, options[OptSetVbiOutFormat],
		      options[OptTryVbiOutFormat],
		      V4L2_BUF_TYPE_VBI_OUTPUT, raw_fmt_out);
}

static void __vbi_get(cv4l_fd &_fd, __u32 type, v4l2_format &fmt)
{
	int fd = _fd.g_fd();

	fmt.type = type;
	if (doioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
		printfmt(fd, fmt);
}

void vbi_get(cv4l_fd &_fd)
{
	if (options[OptGetSlicedVbiFormat])
		__vbi_get(_fd, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE, sliced_fmt);
	if (options[OptGetSlicedVbiOutFormat])
		__vbi_get(_fd, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT, sliced_fmt_out);
	if (options[OptGetVbiFormat])
		__vbi_get(_fd, V4L2_BUF_TYPE_VBI_CAPTURE, raw_fmt);
	if (options[OptGetVbiOutFormat])
		__vbi_get(_fd, V4L2_BUF_TYPE_VBI_OUTPUT, raw_fmt_out);
}

static void __vbi_list(cv4l_fd &fd, __u32 type)
{
	struct v4l2_sliced_vbi_cap cap;

	cap.type = type;
	if (doioctl(fd.g_fd(), VIDIOC_G_SLICED_VBI_CAP, &cap) == 0)
		print_sliced_vbi_cap(cap);
}

void vbi_list(cv4l_fd &fd)
{
	if (options[OptGetSlicedVbiCap])
		__vbi_list(fd, V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
	if (options[OptGetSlicedVbiOutCap])
		__vbi_list(fd, V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
}

#include <cctype>

#include "v4l2-ctl.h"

__u32 parse_pixelformat(const char *value)
{
	bool be = strlen(value) == 7 && !memcmp(value + 4, "-BE", 3);

	if (be || strlen(value) == 4) {
		__u32 pixelformat = v4l2_fourcc(value[0], value[1], value[2], value[3]);

		return be ? pixelformat | (1U << 31) : pixelformat;
	}
	if (isdigit(value[0]))
		return strtoul(value, nullptr, 0);

	fprintf(stderr, "The pixelformat '%s' is invalid\n", value);
	std::exit(EXIT_FAILURE);
}

int video_get_and_update_fmt(cv4l_fd &fd, struct v4l2_format &vfmt,
			     __u32 type, __u32 magic, video_format_request &request)
{
	cv4l_fmt fmt(type);
	bool output = type == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
		      type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	bool mplane = V4L2_TYPE_IS_MULTIPLANAR(type);
	unsigned fields = request.fields;

	fmt.fmt.pix.priv = magic;
	int ret = doioctl(fd.g_fd(), VIDIOC_G_FMT, &fmt);

	if (ret)
		return ret;

	if (fields & FmtWidth)
		fmt.s_width(request.width);
	if (fields & FmtHeight)
		fmt.s_height(request.height);
	if (fields & FmtPixelFormat) {
		if (request.pixelformat < 256)
			request.pixelformat = find_pixel_format(fd.g_fd(),
					request.pixelformat, output, mplane);
		fmt.s_pixelformat(request.pixelformat);
	}
	if (fields & FmtField)
		fmt.s_field(request.field);
	if (fields & FmtFlags)
		fmt.s_flags(request.flags);
	if (fields & FmtBytesPerLine) {
		for (unsigned i = 0; i < (mplane ? VIDEO_MAX_PLANES : 1); i++)
			fmt.s_bytesperline(request.bytesperline[i], i);
	} else {
		// Let the driver recalculate the stride for the new width.
		for (unsigned i = 0; i < fmt.g_num_planes(); i++)
			fmt.s_bytesperline(0, i);
	}
	if (fields & FmtSizeImage) {
		for (unsigned i = 0; i < (mplane ? VIDEO_MAX_PLANES : 1); i++)
			fmt.s_sizeimage(request.sizeimage[i], i);
	}
	if (fields & FmtColorspace)
		fmt.s_colorspace(request.colorspace);
	if (fields & FmtYCbCr)
		fmt.s_ycbcr_enc(request.ycbcr);
	if (fields & FmtQuantization)
		fmt.s_quantization(request.quantization);
	if (fields & FmtXferFunc)
		fmt.s_xfer_func(request.xfer_func);
	if (!output && (fields & (FmtColorspace | FmtYCbCr |
				 FmtQuantization | FmtXferFunc)))
		fmt.s_flags(fmt.g_flags() | V4L2_PIX_FMT_FLAG_SET_CSC);

	vfmt = fmt;
	if ((fields & FmtPixelFormat) &&
	    !valid_pixel_format(fd.g_fd(), request.pixelformat, output, mplane)) {
		if (request.pixelformat)
			fprintf(stderr, "The pixelformat '%s' is invalid\n",
				fcc2s(request.pixelformat).c_str());
		else
			fprintf(stderr, "The pixelformat index was invalid\n");
		return -EINVAL;
	}
	return 0;
}

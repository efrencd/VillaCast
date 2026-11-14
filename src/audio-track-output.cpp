// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include <obs.h>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QUdpSocket>
#include <QtEndian>
#include <array>
#include <cstring>

namespace {
struct AudioOutput {
	obs_output_t *output;
	QUdpSocket socket;
	quint32 ssrc = QRandomGenerator::global()->generate();
	quint32 timestampOffset = QRandomGenerator::global()->generate();
	quint16 sequence = static_cast<quint16>(QRandomGenerator::global()->generate());
	bool first = true;

	explicit AudioOutput(obs_output_t *output) : output(output) {}
};

void *create(obs_data_t *settings, obs_output_t *output)
{
	auto *context = new AudioOutput(output);
	const auto port = static_cast<quint16>(obs_data_get_int(settings, "port"));
	context->socket.connectToHost(QHostAddress::LocalHost, port, QIODevice::WriteOnly);
	if (context->socket.state() != QAbstractSocket::ConnectedState) {
		delete context;
		return nullptr;
	}
	return context;
}

bool start(void *data)
{
	auto &context = *static_cast<AudioOutput *>(data);
	return obs_output_can_begin_data_capture(context.output, 0) &&
	       obs_output_initialize_encoders(context.output, 0) && obs_output_begin_data_capture(context.output, 0);
}

void packet(void *data, encoder_packet *encoded)
{
	if (!encoded || !encoded->size || encoded->size > 4096 || encoded->timebase_den <= 0)
		return;
	auto &context = *static_cast<AudioOutput *>(data);
	std::array<char, 12 + 4096> rtp;
	rtp[0] = char(0x80);
	rtp[1] = char(111 | (context.first ? 0x80 : 0));
	context.first = false;
	const auto timestamp = encoded->dts * encoded->timebase_num * 48000 / encoded->timebase_den;
	qToBigEndian(context.sequence++, rtp.data() + 2);
	qToBigEndian(context.timestampOffset + static_cast<quint32>(timestamp), rtp.data() + 4);
	qToBigEndian(context.ssrc, rtp.data() + 8);
	std::memcpy(rtp.data() + 12, encoded->data, encoded->size);
	// The encoder callback runs off the Qt thread. Use only the native socket here.
#ifdef _WIN32
	::send(static_cast<SOCKET>(context.socket.socketDescriptor()), rtp.data(), static_cast<int>(encoded->size + 12),
	       0);
#else
	::send(static_cast<int>(context.socket.socketDescriptor()), rtp.data(), encoded->size + 12, MSG_DONTWAIT);
#endif
}
} // namespace

void registerAudioTrackOutput()
{
	obs_output_info info{};
	info.id = "villacast_audio_rtp";
	info.flags = OBS_OUTPUT_AUDIO | OBS_OUTPUT_ENCODED;
	info.get_name = [](void *) {
		return "VillaCast audio RTP";
	};
	info.create = create;
	info.destroy = [](void *data) {
		delete static_cast<AudioOutput *>(data);
	};
	info.start = start;
	info.stop = [](void *data, uint64_t) {
		obs_output_end_data_capture(static_cast<AudioOutput *>(data)->output);
	};
	info.encoded_packet = packet;
	info.encoded_audio_codecs = "opus";
	obs_register_output(&info);
}

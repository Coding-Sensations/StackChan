"""Source contract regression: DVP completion time never proves exposure time."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[3]


class CaptureTimingContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        def source(path):
            # The worktree intentionally sparsely checks out host files. Read
            # the firmware contract from this exact candidate's Git tree.
            return subprocess.run(
                ['git', 'show', f'HEAD:{path}'], cwd=ROOT, check=True,
                capture_output=True, text=True,
            ).stdout

        cls.clock = source('firmware/main/companion/camera_capture_clock.hpp')
        cls.producer = source('firmware/main/hal/board/stackchan_camera.cc')
        cls.contract = source('firmware/components/espressif__esp_video/include/esp_video_capture_clock_contract.h')
        cls.ioctl = source('firmware/components/espressif__esp_video/src/esp_video_ioctl.c')
        cls.driver = source('firmware/components/espressif__esp_video/src/device/esp_video_dvp_device.c')
        cls.buffer_publish = source('firmware/components/espressif__esp_video/src/esp_video.c')

    def test_capture_origin_is_explicitly_unqualified_without_attended_authority(self):
        self.assertIn('static constexpr bool CaptureOriginQualified() noexcept {return false;}', self.clock)
        observe = self.clock.split('AttestedCaptureClock ObserveDequeued', 1)[1].split('bool RawObservation', 1)[0]
        self.assertIn('!CaptureOriginQualified()', observe)
        self.assertIn('!external_timing_', observe)
        self.assertIn('!raw.driver_sequence', observe)
        self.assertIn('revision!=ESP_VIDEO_CAPTURE_CLOCK_CONTRACT_REVISION', observe)
        self.assertIn('return {};', observe)

    def test_driver_timestamp_is_snapshotted_before_dma_buffer_requeue(self):
        capture = self.producer.split('bool StackChanCamera::CaptureBound', 1)[1].split('bool StackChanCamera::CopyLV01', 1)[0]
        observation = capture.index('DriverCaptureObservation observation')
        requeue = capture.index('VIDIOC_QBUF')
        clock = capture.index('ObserveDequeued(binding,observation)')
        self.assertLess(observation, requeue)
        self.assertLess(requeue, clock)
        self.assertIn('PublishAttestedLocked(binding,clock,true', capture)
        self.assertIn('stays unavailable until that driver contract is qualified', capture)

    def test_dvp_completion_time_is_sequenced_and_published_before_wakeup(self):
        callback = self.driver.split('dvp_video_on_trans_finished', 1)[1].split('dvp_video_on_get_new_trans', 1)[0]
        self.assertIn('const int64_t captured_us = esp_timer_get_time()', callback)
        self.assertIn('capture_clock_active', callback)
        self.assertIn('esp_video_done_buffer_capture_clock', callback)
        publish = self.buffer_publish.split('esp_video_done_buffer_capture_clock', 1)[1].split('return ESP_OK;', 1)[0]
        self.assertIn('element->capture_clock_us = captured_us;', publish)
        self.assertIn('element->capture_clock_sequence = ++stream->capture_clock_sequence;', publish)
        self.assertIn('element->capture_clock_revision = ESP_VIDEO_CAPTURE_CLOCK_CONTRACT_REVISION;', publish)
        self.assertLess(publish.index('ELEMENT_SET_ALLOCATED(element)'), publish.index('xSemaphoreGive'))

    def test_contract_names_dma_completion_not_exposure_start(self):
        self.assertIn('DVP EOF/DMA transfer completion', self.contract)
        self.assertIn('NOT exposure-start time', self.contract)

    def test_v4l2_exports_only_stamped_completion_values_not_exposure_proof(self):
        self.assertIn('uint32_t esp_video_capture_clock_contract_revision(void);', self.contract)
        self.assertIn('vbuf->timestamp.tv_sec = 0;', self.ioctl)
        self.assertIn('vbuf->sequence = 0;', self.ioctl)
        self.assertIn('element->capture_clock_us && element->capture_clock_sequence', self.ioctl)
        self.assertIn('vbuf->timestamp.tv_sec = element->capture_clock_us / 1000000;', self.ioctl)
        self.assertIn('vbuf->sequence = element->capture_clock_sequence;', self.ioctl)


if __name__ == '__main__':
    unittest.main()

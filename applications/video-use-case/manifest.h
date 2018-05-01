#ifndef __MANIFEST_H__
#define __MANIFEST_H__

#include <vector>
#include <string>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>

using namespace std;

// Note: the following manifest generation tools only works with the manfiest
// of following format:
//
// <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
// <MPD id="79726b98-dc87-4eef-9880-85a77b0ea937" profiles="urn:mpeg:dash:profile:isoff-main:2011" type="static" availabilityStartTime="2016-02-27T04:10:06.000Z" publishTime="2016-02-27T04:10:17.000Z" mediaPresentationDuration="P0Y0M0DT0H1M24.000S" minBufferTime="P0Y0M0DT0H0M1.000S" bitmovin:version="1.7.0" xmlns:ns2="http://www.w3.org/1999/xlink" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:bitmovin="http://www.bitmovin.net/mpd/2015">
//    <Period>
//        <AdaptationSet mimeType="video/mp4">
//            <Representation id="480_1200000" bandwidth="1200000" width="854" height="480" frameRate="30" codecs="avc1.64001F">
//            	<SegmentTemplate media="480_1200000/dash/segment_$Number$.m4s" initialization="480_1200000/dash/init.mp4" duration="114750" startNumber="0" timescale="30000"/>
//            </Representation>
//            <Representation id="480_800000" bandwidth="800000" width="854" height="480" frameRate="30" codecs="avc1.42001F">
//            	<SegmentTemplate media="480_800000/dash/segment_$Number$.m4s" initialization="480_800000/dash/init.mp4" duration="114750" startNumber="0" timescale="30000"/>
//            </Representation>
//            <Representation id="480_400000" bandwidth="400000" width="854" height="480" frameRate="30" codecs="avc1.42001F">
//            	<SegmentTemplate media="480_400000/dash/segment_$Number$.m4s" initialization="480_400000/dash/init.mp4" duration="114750" startNumber="0" timescale="30000"/>
//            </Representation>
//        </AdaptationSet>
//    </Period>
//</MPD>
//
// Essentially it takes this file and first convert to equivalent manfiest file without the
// segment template to one where segment template is replaced by list of actual chunk URIs.
// Then it replace each chunk URI with the XIA DAG url.


/* manifest related constants */
const char REPRESENTATION[] = "Representation";

const char SEGMENT_LIST[] = "SegmentList";
const char SEGMENT_URL[] = "SegmentURL";
const char SEGMENT_BASE[] = "SegmentBase";
const char SEGMENT_TEMPLATE[] = "SegmentTemplate";

const char SEGMENT_DURATION[] = "duration";
const char SEGMENT_TIMESCALE[] = "timescale";
const char SEGMENT_MEDIA[] = "media";
const char SEGMENT_INITIALIZATION[] = "initialization";
const char SEGMENT_INITIALIZATION_ELEMENT[] = "Initialization";
const char SEGMENT_INITIALIZATION_URL[] = "sourceURL";

const char REPRESENTATION_BANDWIDTH[] = "bandwidth";

/**
 * Given a DASH manifest file with SegmentTemplates, convert it to manifest where segment template
 * is replaced by list of actual chunk URIs.
 */
int parse_dash_manifest(const char *video_folder, const char *from_uri, const char* to_uri);

/**
 * Given a DASH manifest without segment template (processed by above function), replace each segment entry
 * with DAG url of that video chunk.
 */
int generate_XIA_manifest(const char *video_folder, const char *from_uri, const char* to_uri, map<string, vector<string> > & pathToUrl);

#endif

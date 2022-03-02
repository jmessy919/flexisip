#include "b2bua-server.hh"

namespace flexisip {
namespace b2bua {
namespace trenscrypter {

	class encryptionConfiguration {
		friend class Trenscrypter;

		linphone::MediaEncryption mode;
		std::regex pattern; /**< regular expression applied on the callee sip address, when matched, the associated mediaEncryption mode is used on the output call */
		std::string stringPattern; /**< a string version of the pattern for log purpose as the std::regex does not carry it*/

	public:
		encryptionConfiguration(linphone::MediaEncryption p_mode, std::string p_pattern): mode(p_mode), pattern(p_pattern), stringPattern(p_pattern) {};
	};

	class srtpConfiguration {
		friend class Trenscrypter;

		std::list<linphone::SrtpSuite> suites;
		std::regex pattern; /**< regular expression applied on the callee sip address, when matched, the associated SRTP suites are used */
		std::string stringPattern;/**< a string version of the pattern for log purposes as the std::regex does not carry it */

	public:
		srtpConfiguration(std::list<linphone::SrtpSuite> p_suites, std::string p_pattern): suites(p_suites), pattern(p_pattern), stringPattern(p_pattern) {};
	};


	/**
	 * Media encryption transcoder
	 */
	class Trenscrypter : public IModule {
		std::shared_ptr<linphone::Core> mCore;
		std::list<encryptionConfiguration> mOutgoingEncryption;
		std::list<srtpConfiguration> mSrtpConf;

	public:
		void init(const std::shared_ptr<linphone::Core>& core, const flexisip::GenericStruct& config) override;
		linphone::Reason onCallCreate(linphone::CallParams& outgoingCallParams, const linphone::Call& incomingCall) override;
	};

}}} // flexisip::b2bua::trenscrypter
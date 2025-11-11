import React from 'react';
import './Sponsors.css';

import sponsor1 from '../../assets/Frederal_bank.svg';
import sponsor2 from '../../assets/Radio_mango.svg';
import sponsor3 from '../../assets/project_2.svg';
import sponsor4 from '../../assets/project_2.svg';

const Sponsors = () => {
    return (
        <div id="sponsors" className="sponsors">
            <div className="sponsors-title">
                <h1>Our Sponsors</h1>
                <p>We thank our generous sponsors for their continued support</p>
            </div>

            <div className="sponsors-logos">
                <img src={sponsor1} alt="Sponsor 1" />
                <img src={sponsor2} alt="Sponsor 2" />
                {/*  <img src={sponsor3} alt="Sponsor 3" />
                <img src={sponsor4} alt="Sponsor 4" /> */}
            </div>
        </div>
    );
};

export default Sponsors;
